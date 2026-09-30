/*
 * stepper.h
 *
 * Driver para un motor paso a paso bipolar controlado mediante un A4988
 * utilizando la EDU-CIAA-NXP y la sAPI.
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
 */

#ifndef _STEPPER_H_
#define _STEPPER_H_

/*==================[inclusions]=============================================*/

#include "sapi_datatypes.h"
#include "sapi_peripheral_map.h"

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

/*
 * Timer utilizado para generar los pulsos STEP.
 *
 * El timer dispone de un Compare Match 0 que determina el periodo del
 * tren de pulsos y un Compare Match 1 que determina cuando termina el
 * pulso HIGH de STEP.
 */
#ifndef STEPPER_TIMER
#define STEPPER_TIMER         TIMER1
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

/*==================[tipos]==================================================*/

typedef enum {
   STEPPER_DIRECTION_FORWARD = 0,
   STEPPER_DIRECTION_REVERSE = 1
} stepperDirection_t;

/*
 * Estado interno del driver.
 *
 * Se agrupan en una sola estructura todas las variables que pertenecen al
 * stepper. Actualmente el modulo trabaja con una unica instancia, por lo
 * que las funciones publicas no necesitan recibir un puntero al objeto.
 *
 * La estructura deja agrupado el estado y facilita una futura extension a
 * varias instancias sin volver a repartir las variables globales entre
 * distintas partes del archivo.
 */
typedef struct {
   /* Pines fisicos utilizados por el driver. */
   gpioMap_t stepPin;
   gpioMap_t dirPin;
   gpioMap_t enablePin;

   /* Timer utilizado para generar el tren de pulsos STEP. */
   timerMap_t timer;

   /* Cantidad de pulsos que aun falta generar en el movimiento actual. */
   volatile uint32_t remainingSteps;

   /* TRUE mientras existe un movimiento en curso. */
   volatile bool_t moving;

   /* Velocidad configurada en pulsos STEP por segundo. */
   uint32_t stepsPerSecond;

   /* Sentido actualmente configurado. */
   stepperDirection_t direction;

   /* Estado de inicializacion del modulo. */
   bool_t initialized;

   /* Estado logico del pin ENABLE: TRUE si el driver esta habilitado. */
   bool_t enabled;

} stepper_t;

/*==================[funciones publicas]=====================================*/

/*
 * Inicializa STEP, DIR y ENABLE como salidas digitales y establece el
 * estado inicial del driver.
 *
 * La inicializacion deja el driver HABILITADO, por lo que no es necesario
 * llamar a stepperEnable() inmediatamente despues de stepperInit().
 *
 * El movimiento aun no comienza durante la inicializacion.
 */
bool_t stepperInit( void );

/*
 * Habilita fisicamente las salidas del A4988.
 * ENABLE es activo en LOW, por lo que esta funcion escribe LOW en ese pin.
 */
bool_t stepperEnable( void );

/*
 * Detiene cualquier movimiento en curso y deshabilita las salidas del
 * A4988. ENABLE queda en HIGH.
 */
bool_t stepperDisable( void );

/*
 * Configura el sentido logico del motor.
 *
 * No se permite cambiar DIR mientras un movimiento esta en curso, porque
 * el A4988 toma el sentido en el siguiente flanco ascendente de STEP.
 */
bool_t stepperSetDirection( stepperDirection_t direction );

/*
 * Configura la velocidad del tren de pulsos STEP.
 *
 * El parametro representa la cantidad de pulsos STEP enviados por segundo.
 * Ejemplo:
 *
 *       stepperSetSpeed(1000);
 *
 * configura 1000 pasos/microsteps por segundo.
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
 *
 * La funcion solamente programa el timer y retorna; los pulsos son
 * generados posteriormente desde las interrupciones del timer.
 */
bool_t stepperMove( uint32_t steps );

/*
 * Detiene el movimiento actual, coloca STEP en LOW y libera el timer.
 *
 * La funcion NO deshabilita el A4988. Para deshabilitar el driver tambien
 * debe utilizarse stepperDisable().
 */
void stepperStop( void );

/*
 * Retorna TRUE mientras exista un movimiento en curso y FALSE cuando no
 * quedan pulsos por generar.
 */
bool_t stepperIsBusy( void );

/*==================[callbacks internos expuestos a sAPI Timer]=============*/

/*
 * Compare Match 0:
 * genera el flanco ascendente de STEP y consume un pulso del movimiento.
 */
void stepperTimerCompareMatch0func( void* ptr );

/*
 * Compare Match 1:
 * genera el flanco descendente de STEP y, si fue el ultimo pulso, termina
 * el movimiento y libera el timer.
 */
void stepperTimerCompareMatch1func( void* ptr );

/*==================[c++]====================================================*/
#ifdef __cplusplus
}
#endif

/*==================[end of file]============================================*/
#endif /* _STEPPER_H_ */
