/*
 * stepper.h
 *
 * Driver para un motor paso a paso bipolar controlado mediante un A4988
 * utilizando la EDU-CIAA-NXP, la sAPI y FreeRTOS.
 *
 * El A4988 se encarga internamente de:
 *   - secuenciar las corrientes de las dos fases,
 *   - realizar el microstepping seleccionado mediante MS1/MS2/MS3,
 *   - regular la corriente de las bobinas.
 *
 * La EDU-CIAA solamente debe:
 *   - habilitar/deshabilitar el driver mediante ENABLE,
 *   - seleccionar el sentido mediante DIR,
 *   - generar los pulsos de STEP.
 *
 * REPARTO DE TAREAS CON FreeRTOS
 *
 * Los flancos de STEP se generan dentro de las interrupciones del timer
 * (Compare Match 0 -> flanco ascendente, Compare Match 1 -> flanco
 * descendente). No se generan desde una task porque el tiempo que tarda
 * FreeRTOS en despertar una task (cambio de contexto + planificador) no
 * esta acotado y puede superar facilmente los 2 us de HIGH/LOW que se
 * utilizan aqui.
 *
 * Una task propia del modulo (creada en stepperInit()) es la duena del
 * timer: es la unica que ejecuta Timer_Init() y Timer_DeInit(). La task
 * duerme esperando notificaciones y se despierta:
 *   - cuando stepperMove() acepta un movimiento (arma el timer),
 *   - cuando el movimiento termina o es cancelado por stepperStop()
 *     (libera el timer).
 *
 * Las funciones publicas estan pensadas para ser llamadas desde otras
 * tasks (por ejemplo rotator.c) una vez iniciado el scheduler. Ninguna
 * debe llamarse desde una interrupcion.
 *
 * PERFIL DE VELOCIDAD (RAMPA)
 *
 * Un motor paso a paso no puede arrancar de golpe a cualquier velocidad:
 * por encima de cierta frecuencia el rotor no logra sincronizarse con los
 * pulsos y solo vibra. Por eso stepperMove() no genera los pulsos a
 * frecuencia constante sino con un perfil trapezoidal:
 *
 *       velocidad
 *           ^
 *           |        ___________________   <- stepperSetSpeed()
 *           |       /                   \_
 *           |      /                      \_
 *           | ____/                         \____
 *           |  STEPPER_START_SPS
 *           +-----------------------------------> tiempo
 *             aceleracion   crucero    frenado
 *
 * stepperSetSpeed() fija la velocidad MAXIMA (de crucero). La rampa la
 * gestiona el modulo internamente: el usuario no necesita ninguna otra
 * funcion. La aceleracion y la velocidad inicial dependen del motor y de
 * la carga, por lo que son parametros de compilacion (STEPPER_ACCEL_SPS2 y
 * STEPPER_START_SPS).
 *
 * Si el movimiento es demasiado corto para llegar a la velocidad
 * configurada, el perfil es triangular y la velocidad maxima realmente
 * alcanzada es:
 *
 *       vmax = sqrt( vstart^2 + aceleracion * pasos )
 */

#ifndef _STEPPER_H_
#define _STEPPER_H_

/*==================[inclusions]=============================================*/

#include "sapi_datatypes.h"
#include "sapi_peripheral_map.h"

#include "FreeRTOS.h"
#include "task.h"

/*==================[c++]====================================================*/
#ifdef __cplusplus
extern "C" {
#endif

/*==================[configuracion de hardware]===============================*/

/*
 * Estas tres definiciones deben coincidir con el cableado real entre la
 * EDU-CIAA-NXP y el modulo A4988.
 *
 * STEP   -> entrada STEP del A4988
 * DIR    -> entrada DIR del A4988
 * ENABLE -> entrada ENABLE del A4988, activa en LOW
 *
 * RESET y SLEEP no son controlados por este modulo. Para funcionamiento
 * normal, ambos deben permanecer en HIGH.
 *
 * MS1, MS2 y MS3 tampoco son controlados por software en este modulo. Su
 * estado se fija por hardware segun el microstepping deseado.
 *
 * PINES FIJOS (VCC / GND)
 *
 * Cualquiera de los tres pines puede definirse como VCC o GND cuando esa
 * entrada del A4988 esta cableada directamente a la alimentacion o a masa
 * en lugar de a un GPIO de la EDU-CIAA. Por ejemplo:
 *
 *       #define STEPPER_ENABLE_PIN   GND    (driver siempre habilitado)
 *       #define STEPPER_DIR_PIN      GND    (sentido siempre FORWARD)
 *
 * La sAPI devuelve FALSE tanto en gpioConfig() como en gpioWrite() cuando
 * el pin es VCC o GND, y ese FALSE no se distingue de un error real. Por
 * eso este modulo detecta en tiempo de compilacion si un pin es fijo y, en
 * ese caso, directamente no llama a gpioConfig()/gpioWrite() sobre el.
 * Asi, un FALSE de la sAPI sobre un GPIO real siempre es un error.
 *
 * Que ocurre con cada pin fijo:
 *   - STEP fijo   : no hay forma de generar pulsos, el motor no se movera
 *                   (se emite un #warning).
 *   - DIR fijo    : GND implica FORWARD y VCC implica REVERSE.
 *                   stepperSetDirection() devuelve TRUE si se pide el
 *                   sentido que el cableado ya impone y FALSE si se pide
 *                   el contrario.
 *   - ENABLE fijo : GND implica driver siempre habilitado y VCC siempre
 *                   deshabilitado (se emite un #warning).
 *                   Ver stepperEnable() y stepperDisable().
 */
#ifndef STEPPER_STEP_PIN
#define STEPPER_STEP_PIN      GPIO2
#endif

#ifndef STEPPER_DIR_PIN
#define STEPPER_DIR_PIN       GPIO3
#endif

#ifndef STEPPER_ENABLE_PIN
#define STEPPER_ENABLE_PIN    T_FIL2
#endif

/* Grados por paso completo del motor (NEMA de 200 pasos/vuelta). Lo usa rotator.c. */
#define STEPPER_STEP_ANGLE_DEG   1.8f

/* Microstepping fijado por hardware con MS1/MS2/MS3 del A4988: 1, 2, 4, 8 o 16. Lo usa rotator.c. */
#ifndef STEPPER_MICROSTEPS
#define STEPPER_MICROSTEPS       1
#endif

/*
 * Timer utilizado para generar los pulsos STEP.
 *
 * El timer dispone de un Compare Match 0 que determina el periodo del
 * tren de pulsos y un Compare Match 1 que determina cuando termina el
 * pulso HIGH de STEP.
 */
#ifndef STEPPER_TIMER
#define STEPPER_TIMER         TIMER0
#endif

/*==================[deteccion de pines fijos VCC / GND]=====================*/

/*
 * VCC y GND son constantes de un enum (gpioMap_t), por lo que el
 * preprocesador no puede compararlas directamente. Para detectarlas se
 * pega el nombre del pin a un prefijo:
 *
 *       STEPPER_PINKIND( VCC )   ->  STEPPER_PINKIND_VCC   ->  1
 *       STEPPER_PINKIND( GND )   ->  STEPPER_PINKIND_GND   ->  2
 *       STEPPER_PINKIND( GPIO2 ) ->  STEPPER_PINKIND_GPIO2 ->  (no definido)
 *
 * Un identificador no definido vale 0 dentro de un #if, de modo que
 * cualquier pin que no sea VCC ni GND queda como pin comun.
 *
 * La doble indireccion (STEPPER_CAT -> STEPPER_CAT_) es necesaria para que
 * STEPPER_xxx_PIN se expanda antes de realizar el pegado.
 */
#define STEPPER_PINKIND_VCC      1
#define STEPPER_PINKIND_GND      2

#define STEPPER_CAT_( a, b )     a##b
#define STEPPER_CAT( a, b )      STEPPER_CAT_( a, b )
#define STEPPER_PINKIND( pin )   STEPPER_CAT( STEPPER_PINKIND_, pin )

/*
 * Resultado de la deteccion, para cada pin:
 *
 *   STEPPER_xxx_FIXED        1 si el pin esta cableado a VCC o GND, 0 si es
 *                            un GPIO comun.
 *   STEPPER_xxx_FIXED_LEVEL  nivel logico fijo del pin (TRUE = VCC,
 *                            FALSE = GND). Solo existe si FIXED es 1 y solo
 *                            para DIR y ENABLE: un STEP fijo no tiene nivel
 *                            util porque directamente no genera pulsos.
 */
#if   ( STEPPER_PINKIND( STEPPER_STEP_PIN ) == STEPPER_PINKIND_VCC )
   #define STEPPER_STEP_FIXED         1
#elif ( STEPPER_PINKIND( STEPPER_STEP_PIN ) == STEPPER_PINKIND_GND )
   #define STEPPER_STEP_FIXED         1
#else
   #define STEPPER_STEP_FIXED         0
#endif

#if   ( STEPPER_PINKIND( STEPPER_DIR_PIN ) == STEPPER_PINKIND_VCC )
   #define STEPPER_DIR_FIXED          1
   #define STEPPER_DIR_FIXED_LEVEL    TRUE
#elif ( STEPPER_PINKIND( STEPPER_DIR_PIN ) == STEPPER_PINKIND_GND )
   #define STEPPER_DIR_FIXED          1
   #define STEPPER_DIR_FIXED_LEVEL    FALSE
#else
   #define STEPPER_DIR_FIXED          0
#endif

#if   ( STEPPER_PINKIND( STEPPER_ENABLE_PIN ) == STEPPER_PINKIND_VCC )
   #define STEPPER_ENABLE_FIXED       1
   #define STEPPER_ENABLE_FIXED_LEVEL TRUE
#elif ( STEPPER_PINKIND( STEPPER_ENABLE_PIN ) == STEPPER_PINKIND_GND )
   #define STEPPER_ENABLE_FIXED       1
   #define STEPPER_ENABLE_FIXED_LEVEL FALSE
#else
   #define STEPPER_ENABLE_FIXED       0
#endif

#if STEPPER_STEP_FIXED
   #warning "STEPPER_STEP_PIN es VCC o GND: no se generaran pulsos STEP"
#endif

#if STEPPER_ENABLE_FIXED && ( STEPPER_ENABLE_FIXED_LEVEL == 1 )
   #warning "STEPPER_ENABLE_PIN es VCC: el A4988 queda deshabilitado (ENABLE es activo en LOW)"
#endif

/*==================[configuracion de la task]===============================*/

/*
 * Prioridad y stack de la task interna del stepper.
 *
 * La task solo arma y libera el timer, por lo que consume poco CPU. Conviene
 * que su prioridad sea MAYOR que la de las tasks que llaman a stepperMove()
 * y stepperStop(): asi el arranque y la liberacion del timer ocurren apenas
 * se la notifica y stepperStop() retorna con el timer ya liberado.
 */
#ifndef STEPPER_TASK_PRIORITY
#define STEPPER_TASK_PRIORITY     ( tskIDLE_PRIORITY + 3 )
#endif

#ifndef STEPPER_TASK_STACK_SIZE
#define STEPPER_TASK_STACK_SIZE   ( configMINIMAL_STACK_SIZE * 2 )
#endif

/*
 * Tiempo maximo, en milisegundos, que stepperStop() espera a que la task
 * termine de liberar el timer cuando esta tiene menor prioridad que quien
 * llama.
 */
#ifndef STEPPER_STOP_TIMEOUT_MS
#define STEPPER_STOP_TIMEOUT_MS   100
#endif

/*==================[temporizacion del STEP]=================================*/

/*
 * El datasheet del A4988 establece como minimo:
 *   - 1 us con STEP en HIGH
 *   - 1 us con STEP en LOW
 *
 * Se utiliza el doble del minimo especificado para ambos estados para
 * disponer de un margen temporal adicional.
 *
 * Por lo tanto, el menor periodo de STEP admitido por este modulo es:
 *
 *       2 us HIGH + 2 us LOW = 4 us
 *
 * Esto establece solamente un limite temporal del generador de pulsos;
 * no implica que el motor pueda funcionar mecanicamente a esa frecuencia.
 */
#define STEPPER_STEP_HIGH_US  2UL
#define STEPPER_STEP_LOW_US   2UL

/*==================[perfil de velocidad]====================================*/

/*
 * Maxima cantidad de pulsos STEP por segundo que admite este modulo. Sale
 * del periodo minimo de 4 us (2 us HIGH + 2 us LOW): 250000 pasos/s.
 *
 * Es el limite del generador de pulsos. No es el limite del motor (un NEMA
 * con rampa y 12-24 V llega tipicamente a pocos miles de pasos/s) ni
 * necesariamente el que la CPU pueda sostener atendiendo dos interrupciones
 * por pulso.
 */
#define STEPPER_MAX_SPS       ( 1000000UL / ( STEPPER_STEP_HIGH_US + STEPPER_STEP_LOW_US ) )

/*
 * Velocidad con la que el motor puede arrancar (y detenerse) sin rampa, en
 * pasos/s. Los movimientos comienzan y terminan a esta velocidad. Si
 * stepperSetSpeed() recibe una velocidad menor o igual, el movimiento se
 * hace a velocidad constante y sin rampa.
 *
 * Los valores por defecto son conservadores y estan pensados para un NEMA
 * de paso completo sin carga. Se escalan con STEPPER_MICROSTEPS porque se
 * expresan en pasos/microsteps. Deben ajustarse al motor real.
 */
#ifndef STEPPER_START_SPS
#define STEPPER_START_SPS     ( 400UL * STEPPER_MICROSTEPS )
#endif

/*
 * Aceleracion y frenado de la rampa, en pasos/s^2. Con el valor por defecto
 * (10000), pasar de 400 a 1500 pasos/s lleva unos 110 ms y ~104 pasos.
 */
#ifndef STEPPER_ACCEL_SPS2
#define STEPPER_ACCEL_SPS2    ( 10000UL * STEPPER_MICROSTEPS )
#endif

#if ( STEPPER_START_SPS < 1 ) || ( STEPPER_START_SPS > STEPPER_MAX_SPS )
   #error "STEPPER_START_SPS debe estar entre 1 y STEPPER_MAX_SPS"
#endif

#if ( STEPPER_ACCEL_SPS2 < 1 )
   #error "STEPPER_ACCEL_SPS2 debe ser mayor que 0"
#endif

/*==================[tipos]==================================================*/

typedef enum {
   STEPPER_DIRECTION_FORWARD = 0,
   STEPPER_DIRECTION_REVERSE = 1
} stepperDirection_t;


/*==================[funciones publicas]=====================================*/

/*
 * Inicializa STEP, DIR y ENABLE como salidas digitales, establece el
 * estado inicial del driver y crea la task interna del modulo.
 *
 * Puede llamarse antes de vTaskStartScheduler(). Los pines definidos como
 * VCC o GND no se configuran (ver "PINES FIJOS"); eso no es un error.
 * Retorna FALSE si falla la configuracion de algun GPIO real, si no se
 * pudo crear la task o si hay un movimiento en curso.
 *
 * La inicializacion deja el driver HABILITADO, por lo que no es necesario
 * llamar a stepperEnable() inmediatamente despues de stepperInit(). La unica
 * excepcion es ENABLE cableado a VCC, que lo deja deshabilitado.
 *
 * El movimiento aun no comienza durante la inicializacion.
 */

bool_t stepperInit( void );

/*
 * Habilita fisicamente las salidas del A4988.
 * ENABLE es activo en LOW, por lo que esta funcion escribe LOW en ese pin.
 *
 * Con ENABLE fijo en GND retorna TRUE sin escribir nada (ya esta
 * habilitado). Con ENABLE fijo en VCC retorna FALSE porque el cableado
 * impide habilitar el driver.
 */
bool_t stepperEnable( void );

/*
 * Detiene cualquier movimiento en curso y deshabilita las salidas del
 * A4988. ENABLE queda en HIGH.
 *
 * Con ENABLE fijo en VCC retorna TRUE (ya estaba deshabilitado). Con ENABLE
 * fijo en GND el movimiento se detiene pero el driver no puede
 * deshabilitarse, por lo que retorna FALSE.
 */
bool_t stepperDisable( void );

/*
 * Configura el sentido logico del motor.
 *
 * No se permite cambiar DIR mientras un movimiento esta en curso, porque
 * el A4988 toma el sentido en el siguiente flanco ascendente de STEP.
 *
 * Con DIR fijo solo se acepta el sentido que impone el cableado
 * (GND -> FORWARD, VCC -> REVERSE).
 */
bool_t stepperSetDirection( stepperDirection_t direction );

/*
 * Configura la velocidad MAXIMA del tren de pulsos STEP.
 *
 * El parametro representa la cantidad de pulsos STEP por segundo que se
 * alcanzan en el tramo de crucero. Ejemplo:
 *
 *       stepperSetSpeed(1000);
 *
 * configura 1000 pasos/microsteps por segundo.
 *
 * Se acepta cualquier valor entre 1 y STEPPER_MAX_SPS. El modulo se ocupa
 * de que el motor pueda seguirlo: stepperMove() acelera desde
 * STEPPER_START_SPS hasta esta velocidad con la aceleracion
 * STEPPER_ACCEL_SPS2 y frena de forma simetrica al final. Con una velocidad
 * menor o igual a STEPPER_START_SPS no hay rampa.
 *
 * Retorna FALSE si la velocidad es 0 o supera STEPPER_MAX_SPS, o si hay un
 * movimiento en curso.
 *
 * IMPORTANTE: es la velocidad maxima, no la garantizada. En un movimiento
 * corto la rampa no llega a completarse (ver "PERFIL DE VELOCIDAD") y,
 * ademas, que el motor pueda seguir fisicamente una velocidad depende de
 * su torque, de la tension de alimentacion y de la carga.
 *
 * La velocidad se configura por separado de stepperMove(), ya que una
 * funcion define "a que velocidad" mover y la otra "cuantos pulsos"
 * generar.
 */
bool_t stepperSetSpeed( uint32_t stepsPerSecond );

/*
 * Inicia un movimiento NO BLOQUEANTE de 'steps' pulsos STEP.
 *
 * IMPORTANTE:
 *   - No habilita automaticamente el A4988.
 *   - Si el driver no esta habilitado, la funcion retorna FALSE.
 *   - Si ya existe un movimiento en curso, retorna FALSE.
 *   - Requiere el scheduler en marcha: el timer lo arma la task interna.
 *
 * La funcion solamente acepta el movimiento, calcula su perfil de
 * velocidad (aceleracion, crucero y frenado) y notifica a la task; el timer
 * lo arma la task y los pulsos son generados posteriormente desde las
 * interrupciones del timer. Cuando retorna TRUE, stepperIsBusy() ya
 * devuelve TRUE.
 */
bool_t stepperMove( uint32_t steps );

/*
 * Detiene el movimiento actual, coloca STEP en LOW y libera el timer, de
 * modo que dejan de producirse interrupciones.
 *
 * Puede llamarse en cualquier momento desde una task: sin movimiento no
 * hace nada, y durante un movimiento corta el tren de pulsos
 * inmediatamente. Retorna una vez que la task libero el timer (o pasados
 * STEPPER_STOP_TIMEOUT_MS ms si la task no llego a ejecutarse).
 *
 * La funcion NO deshabilita el A4988. Para deshabilitar el driver tambien
 * debe utilizarse stepperDisable().
 */
void stepperStop( void );

/*
 * Retorna TRUE mientras exista un movimiento en curso y FALSE cuando no
 * quedan pulsos por generar y el timer ya fue liberado.
 */
bool_t stepperIsBusy( void );

/*==================[c++]====================================================*/
#ifdef __cplusplus
}
#endif

/*==================[end of file]============================================*/
#endif /* _STEPPER_H_ */
