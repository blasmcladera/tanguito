/*
 * stepper.c
 *
 * Driver sencillo para un motor paso a paso bipolar controlado mediante un
 * A4988 usando STEP/DIR, un timer de la sAPI y una task de FreeRTOS.
 *
 * El A4988 se ocupa internamente de la conmutacion de las bobinas, del
 * microstepping y de la regulacion de corriente. La EDU-CIAA solamente
 * genera el tren de pulsos STEP y mantiene configurado DIR/ENABLE.
 *
 * FUNCIONAMIENTO
 *
 *   - Los flancos de STEP los generan las interrupciones del timer, que
 *     son las unicas con una latencia lo bastante baja y estable como para
 *     respetar los 2 us de HIGH/LOW.
 *
 *   - La task del modulo es la duena del timer: ejecuta Timer_Init() al
 *     comenzar un movimiento y Timer_DeInit() al terminarlo. Estas dos
 *     funciones nunca se llaman desde una interrupcion porque Timer_Init()
 *     contiene esperas activas y Timer_DeInit() corta el clock del
 *     periferico, que la rutina de interrupcion de la sAPI todavia usa
 *     despues de ejecutar el callback.
 *
 *   - La comunicacion hacia la task se hace con notificaciones directas
 *     (xTaskNotify / xTaskNotifyFromISR), que no necesitan ni un objeto
 *     extra ni la task de timers de FreeRTOS.
 *
 *   - La velocidad no es constante: cada movimiento sigue un perfil
 *     trapezoidal (aceleracion, crucero, frenado). El periodo de cada pulso
 *     se recalcula en la interrupcion del Compare Match 0, justo despues de
 *     subir STEP, y se carga en el registro de periodo del timer. Ninguna
 *     de estas cuentas usa funciones de FreeRTOS.
 */

/*==================[inclusions]=============================================*/

#include <stddef.h>

#include "FreeRTOS.h"
#include "task.h"

#include "chip.h"

#include "sapi_gpio.h"
#include "sapi_timer.h"
#include "stepper.h"

/*==================[macros y definiciones]==================================*/

/*
 * Margen, en ticks del timer, entre el contador actual y el nuevo periodo
 * cuando la interrupcion modifica el Match 0 (64 ticks = 0,31 us a 204 MHz).
 * Ver stepperRampUpdate().
 */
#define STEPPER_MATCH_GUARD_TICKS   64UL

/* Bits de notificacion que recibe la task. */
#define STEPPER_NOTIFY_START   ( 1UL << 0 )   /* armar el timer              */
#define STEPPER_NOTIFY_END     ( 1UL << 1 )   /* liberar el timer            */

/*
 * Acceso a los pines con compilacion condicional.
 *
 * Para cada pin, segun sea un GPIO real o este fijo en VCC/GND:
 *
 *   STEPPER_xxx_CONFIG()    configura el pin como salida. Con pin fijo no
 *                           toca la sAPI y retorna TRUE.
 *   STEPPER_xxx_WRITE( v )  escribe el nivel v. Con pin fijo no toca la
 *                           sAPI y retorna TRUE.
 *
 * De esta forma, un FALSE devuelto por la sAPI siempre significa un error
 * real y nunca "se intento escribir en VCC o GND".
 */
#if STEPPER_STEP_FIXED
   #define STEPPER_STEP_CONFIG()      ( TRUE )
   #define STEPPER_STEP_WRITE( v )    ( (void)(v), TRUE )
#else
   #define STEPPER_STEP_CONFIG()      gpioConfig( stepper.stepPin, GPIO_OUTPUT )
   #define STEPPER_STEP_WRITE( v )    gpioWrite( stepper.stepPin, (v) )
#endif

#if STEPPER_DIR_FIXED
   #define STEPPER_DIR_CONFIG()       ( TRUE )
   #define STEPPER_DIR_WRITE( v )     ( (void)(v), TRUE )
   /* El sentido queda impuesto por el cableado: LOW = FORWARD, HIGH = REVERSE. */
   #define STEPPER_DIR_FIXED_DIRECTION                                       \
      ( ( STEPPER_DIR_FIXED_LEVEL ) ? STEPPER_DIRECTION_REVERSE              \
                                    : STEPPER_DIRECTION_FORWARD )
#else
   #define STEPPER_DIR_CONFIG()       gpioConfig( stepper.dirPin, GPIO_OUTPUT )
   #define STEPPER_DIR_WRITE( v )     gpioWrite( stepper.dirPin, (v) )
#endif

#if STEPPER_ENABLE_FIXED
   #define STEPPER_ENABLE_CONFIG()    ( TRUE )
   #define STEPPER_ENABLE_WRITE( v )  ( (void)(v), TRUE )
   /* ENABLE es activo en LOW: un pin fijo en GND deja el driver habilitado. */
   #define STEPPER_ENABLE_FIXED_ENABLES   ( !( STEPPER_ENABLE_FIXED_LEVEL ) )
#else
   #define STEPPER_ENABLE_CONFIG()    gpioConfig( stepper.enablePin, GPIO_OUTPUT )
   #define STEPPER_ENABLE_WRITE( v )  gpioWrite( stepper.enablePin, (v) )
#endif

/*==================[tipos internos]=========================================*/

/*
 * Estados del movimiento. Las transiciones validas son:
 *
 *       IDLE --stepperMove()--> STARTING --task arma el timer--> RUNNING
 *       RUNNING --ultimo pulso o stepperStop()--> ENDING
 *       STARTING --stepperStop()--> ENDING
 *       ENDING --task libera el timer--> IDLE
 *
 *   IDLE     : sin movimiento y con el timer liberado.
 *   STARTING : stepperMove() acepto el movimiento; la task todavia no
 *              termino de armar el timer (no se genera ningun pulso).
 *   RUNNING  : el timer esta armado y las interrupciones generan pulsos.
 *   ENDING   : no se generan mas pulsos; falta que la task libere el timer.
 */
typedef enum {
   STEPPER_STATE_IDLE = 0,
   STEPPER_STATE_STARTING,
   STEPPER_STATE_RUNNING,
   STEPPER_STATE_ENDING
} stepperState_t;

/*
 * Estado interno del driver.
 *
 * Se agrupan en una sola estructura todas las variables que pertenecen al
 * stepper. Actualmente el modulo trabaja con una unica instancia, por lo
 * que las funciones publicas no necesitan recibir un puntero al objeto.
 *
 * La estructura es privada de este archivo: nadie fuera del modulo necesita
 * conocerla, y asi stepper.h solo expone la interfaz publica. Deja agrupado
 * el estado y facilita una futura extension a varias instancias.
 *
 * Los campos que comparten las tasks y las interrupciones del timer
 * (state y remainingSteps) son volatile. Las tasks los modifican siempre
 * dentro de una seccion critica de FreeRTOS.
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

   /* Estado del movimiento actual (ver stepperState_t). */
   volatile stepperState_t state;

   /* Velocidad maxima (de crucero) configurada, en pulsos STEP por segundo. */
   uint32_t stepsPerSecond;

   /*
    * Perfil del movimiento actual. Lo calcula stepperMove() al aceptar el
    * movimiento y despues solo lo lee la interrupcion del Compare Match 0.
    * Los periodos estan expresados en ticks del timer.
    */
   uint32_t totalSteps;            /* pulsos totales del movimiento          */
   uint32_t accelSteps;            /* pulsos de la rampa de aceleracion; la
                                      de frenado tiene la misma cantidad    */
   uint32_t startTicks;            /* periodo a la velocidad inicial         */
   uint32_t targetTicks;           /* periodo a la velocidad de crucero      */
   volatile uint32_t periodTicks;  /* periodo cargado hoy en el Match 0      */
   float rampV0Sq;                 /* velocidad inicial al cuadrado          */
   float rampTwoA;                 /* 2 * aceleracion                        */
   float rampTicksPerSecond;       /* frecuencia del timer en ticks/s        */

   /* Estado de inicializacion del modulo. */
   bool_t initialized;

   /* Estado logico del pin ENABLE: TRUE si el driver esta habilitado. */
   bool_t enabled;

   /* Task interna que arma y libera el timer. */
   TaskHandle_t taskHandle;

} stepper_t;

/*==================[estado interno]=========================================*/

/*
 * Todas las variables asociadas al stepper se agrupan dentro de una unica
 * estructura. De esta manera no quedan variables globales independientes
 * repartidas por el modulo y el estado del driver puede identificarse como
 * una sola unidad.
 */
static stepper_t stepper = {
   /* stepPin        */ STEPPER_STEP_PIN,
   /* dirPin         */ STEPPER_DIR_PIN,
   /* enablePin      */ STEPPER_ENABLE_PIN,
   /* timer          */ STEPPER_TIMER,
   /* remainingSteps */ 0,
   /* state          */ STEPPER_STATE_IDLE,
   /* stepsPerSecond */ 1000,
   /* totalSteps     */ 0,
   /* accelSteps     */ 0,
   /* startTicks     */ 0,
   /* targetTicks    */ 0,
   /* periodTicks    */ 0,
   /* rampV0Sq       */ 0.0f,
   /* rampTwoA       */ 0.0f,
   /* rampTicksPerSecond */ 0.0f,
   /* initialized    */ FALSE,
   /* enabled        */ FALSE,
   /* taskHandle     */ NULL
};

/*
 * TRUE mientras la task tiene el timer inicializado (entre Timer_Init() y
 * Timer_DeInit()). Solo la task lee y escribe esta variable, por lo que no
 * necesita proteccion.
 */
static bool_t timerOwned = FALSE;

/*==================[funciones auxiliares]===================================*/

/*
 * Convierte una velocidad expresada en pulsos por segundo a un periodo
 * expresado en ticks del timer, redondeando al entero mas cercano.
 *
 * Se trabaja en ticks y no en microsegundos porque un microsegundo (204
 * ticks) es una resolucion demasiado gruesa a velocidades altas: a 250000
 * pasos/s el periodo es de 4 us y un redondeo de 1 us seria un error del 25%.
 *
 * Requiere 1 <= stepsPerSecond <= STEPPER_MAX_SPS. Con ese rango el periodo
 * obtenido nunca es menor que STEPPER_STEP_HIGH_US + STEPPER_STEP_LOW_US,
 * que es el periodo minimo valido para esta implementacion.
 */
static uint32_t stepperSpsToTicks( uint32_t stepsPerSecond )
{
   const uint32_t ticksPerSecond = Timer_microsecondsToTicks( 1000000UL );

   return ( ticksPerSecond + ( stepsPerSecond / 2 ) ) / stepsPerSecond;
}

/*
 * Raiz cuadrada de un float.
 *
 * En ARM se usa directamente la instruccion de la FPU (VSQRT, 14 ciclos).
 * No se usa sqrtf() porque con -Og el compilador la deja como una llamada
 * a la libreria matematica, que dentro de una interrupcion es innecesaria,
 * mas lenta y, segun la version de newlib, puede ser una rutina por
 * software. Fuera de ARM (por ejemplo al probar el modulo en una PC) se usa
 * la funcion del compilador.
 */
static inline float stepperSqrt( float x )
{
#if defined( __arm__ ) && defined( __ARM_FP )
   float r;
   __asm volatile ( "vsqrt.f32 %0, %1" : "=t"( r ) : "t"( x ) );
   return r;
#else
   return __builtin_sqrtf( x );
#endif
}

/*
 * Devuelve los registros del periferico timer que corresponde a
 * stepper.timer. Se usa para leer el contador (TC) desde la interrupcion,
 * lo que la sAPI no ofrece.
 */
static LPC_TIMER_T* stepperTimerRegs( void )
{
   switch( stepper.timer ) {
   case TIMER1: return LPC_TIMER1;
   case TIMER2: return LPC_TIMER2;
   case TIMER3: return LPC_TIMER3;
   case TIMER0:
   default:     return LPC_TIMER0;
   }
}

/*
 * Calcula el periodo del proximo pulso y lo carga en el Compare Match 0.
 *
 * Se ejecuta en contexto de interrupcion, justo despues de emitir un pulso
 * y con al menos un pulso pendiente. 'remaining' es la cantidad de pulsos
 * que faltan; los ya emitidos son totalSteps - remaining.
 *
 * PERFIL
 *
 * Con aceleracion constante 'a', la velocidad v despues de recorrer 'e'
 * pasos desde una velocidad inicial v0 cumple:
 *
 *       v^2 = v0^2 + 2 * a * e
 *
 * y el periodo del siguiente pulso es T = 1 / v. Aqui 'e' es el numero de
 * pasos recorridos dentro de la rampa, que se obtiene como el menor entre:
 *
 *   - los pulsos ya emitidos          -> rampa de ACELERACION,
 *   - los pulsos que faltan           -> rampa de FRENADO (simetrica),
 *   - accelSteps                      -> tope: se llego a la velocidad
 *                                        de CRUCERO.
 *
 * Con un unico 'e' los tres tramos quedan descritos por la misma formula, y
 * los movimientos cortos (en los que no se llega al crucero) resultan
 * triangulares sin tratamiento aparte.
 *
 * ARITMETICA
 *
 * Se usa float (FPU del Cortex-M4F) porque la alternativa entera tendria
 * que perder precision a velocidades altas: la variacion del periodo entre
 * pulsos consecutivos es una fraccion de tick. El costo es de decenas de
 * ciclos (una raiz y una division, ambas instrucciones de la FPU). Esto requiere compilar con
 * -mfpu=fpv4-sp-d16 y -mfloat-abi=hard, lo mismo que ya exige el port
 * ARM_CM4F de FreeRTOS, que guarda el contexto de la FPU de forma automatica
 * y diferida cuando una interrupcion la usa.
 *
 * CARGA DEL PERIODO
 *
 * El Compare Match 0 reinicia el contador al coincidir (igualdad, no
 * "mayor o igual"). Si el nuevo valor fuera menor que el contador actual,
 * el contador deberia dar toda la vuelta (2^32 ticks, unos 21 s) para volver
 * a coincidir. Por eso el nuevo periodo se lleva, como minimo, a
 * contador + STEPPER_MATCH_GUARD_TICKS. En condiciones normales el contador
 * esta cerca de 0 (acaba de reiniciarse) y la guarda no actua.
 */
static void stepperRampUpdate( uint32_t remaining )
{
   uint32_t e = stepper.totalSteps - remaining;   /* pulsos ya emitidos */
   uint32_t ticks;
   uint32_t counter;

   if( remaining < e ) {
      e = remaining;
   }

   if( e >= stepper.accelSteps ) {
      /* Crucero. Tambien cubre los movimientos sin rampa (accelSteps = 0). */
      ticks = stepper.targetTicks;
   } else {
      float v2 = stepper.rampV0Sq + stepper.rampTwoA * (float)e;

      ticks = (uint32_t)( stepper.rampTicksPerSecond / stepperSqrt( v2 ) );

      /* Nunca mas rapido que la velocidad de crucero configurada. */
      if( ticks < stepper.targetTicks ) {
         ticks = stepper.targetTicks;
      }
   }

   /* Durante el crucero el periodo no cambia: se evita escribir el registro. */
   if( ticks == stepper.periodTicks ) {
      return;
   }

   counter = Chip_TIMER_ReadCount( stepperTimerRegs() );
   if( ticks < ( counter + STEPPER_MATCH_GUARD_TICKS ) ) {
      ticks = counter + STEPPER_MATCH_GUARD_TICKS;
   }

   stepper.periodTicks = ticks;
   Timer_SetCompareMatch( stepper.timer, TIMERCOMPAREMATCH0, ticks );
}

/*
 * Silencia las interrupciones del timer sin liberarlo.
 *
 * Deshabilita los Compare Match 0 y 1, de modo que el timer sigue contando
 * pero no interrumpe mas. No toca el clock del periferico, por lo que es
 * seguro llamarla desde una interrupcion del propio timer o desde una
 * seccion critica, siempre que el timer este armado (estado RUNNING).
 */
static void stepperQuiesceTimer( void )
{
   Timer_DisableCompareMatch( stepper.timer, TIMERCOMPAREMATCH1 );
   Timer_DisableCompareMatch( stepper.timer, TIMERCOMPAREMATCH0 );
}

/*==================[callbacks del timer]===================================*/

/*
 * Compare Match 0:
 *
 * Timer_Init() utiliza el Compare Match 0 para establecer el periodo del
 * timer. Al alcanzar ese instante, el contador se reinicia y comienza un
 * nuevo periodo STEP.
 *
 * Cada vez que este callback se ejecuta se genera el flanco LOW->HIGH de
 * STEP. Ese flanco es interpretado por el A4988 como un nuevo paso o
 * microstep.
 *
 * Despues del flanco, y con tiempo de sobra antes del Compare Match 1,
 * calcula el periodo del pulso siguiente (ver stepperRampUpdate()).
 *
 * Se ejecuta en contexto de interrupcion.
 */
static void stepperTimerCompareMatch0func( void* ptr )
{
   uint32_t remaining;

   /* La sAPI no utiliza el argumento del callback en este modulo. */
   (void)ptr;

   /*
    * Solo se generan pulsos con el movimiento en RUNNING. Si el movimiento
    * ya termino, fue cancelado, o la task todavia no termino de armar el
    * timer (STARTING), no se consume ningun pulso: la cuenta no se pierde,
    * simplemente el pulso se genera en el periodo siguiente.
    */
   if( (stepper.state != STEPPER_STATE_RUNNING) ||
       (stepper.remainingSteps == 0) ) {
      return;
   }

   /* Genera el flanco ascendente que ordena un paso al A4988. */
   (void)STEPPER_STEP_WRITE( TRUE );

   /* Este flanco ya representa uno de los pulsos solicitados. */
   remaining = stepper.remainingSteps - 1;
   stepper.remainingSteps = remaining;

   /*
    * Despues del ultimo pulso no hay periodo siguiente que calcular: el
    * Compare Match 1 se encarga de cerrar el movimiento.
    */
   if( remaining != 0 ) {
      stepperRampUpdate( remaining );
   }
}

/*
 * Compare Match 1:
 *
 * Se programa a STEPPER_STEP_HIGH_US desde el comienzo de cada periodo.
 * Su funcion durante un movimiento normal es bajar STEP para que el
 * ancho del pulso HIGH sea exactamente el tiempo configurado.
 *
 * Cuando el pulso que acaba de terminar era el ultimo, ademas silencia el
 * timer para que dejen de producirse interrupciones y notifica a la task
 * para que lo libere.
 *
 * Se ejecuta en contexto de interrupcion.
 */
static void stepperTimerCompareMatch1func( void* ptr )
{
   BaseType_t higherPriorityTaskWoken = pdFALSE;

   /* La sAPI no utiliza el argumento del callback en este modulo. */
   (void)ptr;

   /* Termina el pulso STEP actual. */
   (void)STEPPER_STEP_WRITE( FALSE );

   /*
    * remainingSteps llega a cero despues del ultimo flanco ascendente.
    * El ultimo pulso, sin embargo, todavia debia completar sus
    * STEPPER_STEP_HIGH_US, y eso es lo que acaba de ocurrir.
    */
   if( (stepper.state == STEPPER_STATE_RUNNING) &&
       (stepper.remainingSteps == 0) ) {

      /* Ya no se necesita ningun Compare Match: no mas interrupciones. */
      stepperQuiesceTimer();

      /* No se generan mas pulsos; falta que la task libere el timer. */
      stepper.state = STEPPER_STATE_ENDING;

      xTaskNotifyFromISR( stepper.taskHandle,
                          STEPPER_NOTIFY_END,
                          eSetBits,
                          &higherPriorityTaskWoken );

      /* Si la task es mas prioritaria que la interrumpida, se ejecuta ya. */
      portYIELD_FROM_ISR( higherPriorityTaskWoken );
   }
}

/*==================[task del stepper]======================================*/

/*
 * Arma el timer para el movimiento aceptado por stepperMove().
 *
 * Si stepperStop() cancelo el movimiento antes de que la task llegara hasta
 * aqui (estado distinto de STARTING), no se toca el timer.
 *
 * Si stepperStop() llega mientras se arma el timer, el estado pasa a ENDING
 * y no se pasa a RUNNING; la notificacion END que envio stepperStop() hace
 * que la task libere el timer a continuacion.
 */
static void stepperTaskStart( void )
{
   uint32_t startTicks;
   bool_t proceed;

   taskENTER_CRITICAL();
   proceed    = ( stepper.state == STEPPER_STATE_STARTING );
   startTicks = stepper.startTicks;
   taskEXIT_CRITICAL();

   if( !proceed ) {
      return;
   }

   /* STEP debe empezar en LOW antes de comenzar un nuevo tren de pulsos. */
   (void)STEPPER_STEP_WRITE( FALSE );

   /*
    * Compare Match 0 define el periodo del tren STEP.
    *
    * Al llegar a Match 0:
    *       STEP -> HIGH
    *       timer counter -> 0
    *
    * Por lo tanto, el primer pulso sale 'startTicks' ticks despues de
    * Timer_Init(), con el periodo de la velocidad inicial. A partir del
    * primer pulso, la interrupcion del Match 0 va modificando este periodo
    * siguiendo el perfil de velocidad (ver stepperRampUpdate()).
    */
   Timer_Init( stepper.timer,
               startTicks,
               stepperTimerCompareMatch0func );

   /*
    * Compare Match 1 ocurre STEPPER_STEP_HIGH_US despues de cada Match 0.
    * Su callback baja STEP y, por lo tanto, fija el ancho HIGH del pulso.
    */
   Timer_EnableCompareMatch( stepper.timer,
                             TIMERCOMPAREMATCH1,
                             Timer_microsecondsToTicks( STEPPER_STEP_HIGH_US ),
                             stepperTimerCompareMatch1func );

   timerOwned = TRUE;

   /*
    * A partir de aqui las interrupciones pueden generar pulsos. Se pasa a
    * RUNNING solo si nadie cancelo el movimiento mientras se armaba el
    * timer.
    */
   taskENTER_CRITICAL();
   if( stepper.state == STEPPER_STATE_STARTING ) {
      stepper.state = STEPPER_STATE_RUNNING;
   }
   taskEXIT_CRITICAL();
}

/*
 * Libera el timer y deja el modulo listo para un nuevo movimiento.
 *
 * Se ejecuta tanto al terminar el ultimo pulso (notificacion enviada desde
 * el Compare Match 1) como al cancelar con stepperStop().
 */
static void stepperTaskEnd( void )
{
   if( timerOwned ) {
      /* Idempotente: si ya estaba silenciado no pasa nada. */
      stepperQuiesceTimer();

      /* Se detiene y libera el timer utilizado para generar STEP. */
      Timer_DeInit( stepper.timer );
      timerOwned = FALSE;
   }

   /* STEP siempre debe quedar en LOW al terminar el tren de pulsos. */
   (void)STEPPER_STEP_WRITE( FALSE );

   /* Recien ahora se acepta un nuevo movimiento. */
   taskENTER_CRITICAL();
   stepper.remainingSteps = 0;
   stepper.state = STEPPER_STATE_IDLE;
   taskEXIT_CRITICAL();
}

/*
 * Task del stepper.
 *
 * Permanece bloqueada hasta recibir una notificacion. Si llegan START y END
 * juntas (stepperStop() cancelo el movimiento antes de que la task
 * despertara), START se atiende primero y, al encontrar el movimiento
 * cancelado, no arma el timer.
 */
static void stepperTask( void* arg )
{
   uint32_t notified;

   (void)arg;

   for( ;; ) {
      /* Espera sin limite y borra todos los bits al salir. */
      xTaskNotifyWait( 0UL, 0xFFFFFFFFUL, &notified, portMAX_DELAY );

      if( notified & STEPPER_NOTIFY_START ) {
         stepperTaskStart();
      }

      if( notified & STEPPER_NOTIFY_END ) {
         stepperTaskEnd();
      }
   }
}

/*==================[funciones publicas]====================================*/

/*
 * Inicializa el modulo STEP/DIR/ENABLE del A4988 y crea la task interna.
 *
 * No se genera ningun pulso STEP durante esta operacion.
 */
bool_t stepperInit( void )
{
   /* No se reinicializa el modulo con un movimiento en curso. */
   if( stepper.state != STEPPER_STATE_IDLE ) {
      return FALSE;
   }

   /*
    * STEP, DIR y ENABLE son salidas digitales porque los tres pines son
    * controlados por la EDU-CIAA hacia entradas del A4988. Los pines
    * fijos en VCC/GND no se configuran (la macro retorna TRUE).
    */
   if( STEPPER_STEP_CONFIG() == FALSE ) {
      return FALSE;
   }

   if( STEPPER_DIR_CONFIG() == FALSE ) {
      return FALSE;
   }

   if( STEPPER_ENABLE_CONFIG() == FALSE ) {
      return FALSE;
   }

   /*
    * STEP debe comenzar en LOW para evitar generar accidentalmente un
    * flanco ascendente al inicializar el modulo.
    */
   if( STEPPER_STEP_WRITE( FALSE ) == FALSE ) {
      return FALSE;
   }

#if !STEPPER_DIR_FIXED
   /* El sentido por defecto es FORWARD. Con DIR fijo lo impone el cableado. */
   if( STEPPER_DIR_WRITE( FALSE ) == FALSE ) {
      return FALSE;
   }
#endif

   /*
    * La velocidad inicial es de 1000 pulsos/s y no hay movimiento activo.
    */
   stepper.stepsPerSecond = 1000;
   stepper.remainingSteps = 0;

   /*
    * La task se crea una sola vez. Si stepperInit() se llama de nuevo, la
    * task existente se reutiliza.
    */
   if( stepper.taskHandle == NULL ) {
      if( xTaskCreate( stepperTask,
                       "stepper",
                       STEPPER_TASK_STACK_SIZE,
                       NULL,
                       STEPPER_TASK_PRIORITY,
                       &stepper.taskHandle ) != pdPASS ) {
         stepper.taskHandle = NULL;
         return FALSE;
      }
   }

   /*
    * Marcamos el modulo como inicializado antes de llamar a
    * stepperEnable(), ya que esa funcion comprueba este estado.
    */
   stepper.initialized = TRUE;

   /*
    * Se habilita el A4988. Con ENABLE fijo en VCC el driver queda
    * deshabilitado por cableado y eso no es un error de inicializacion.
    */
#if STEPPER_ENABLE_FIXED && !STEPPER_ENABLE_FIXED_ENABLES
   stepper.enabled = FALSE;
#else
   if( !stepperEnable() ) {
      stepper.initialized = FALSE;
      return FALSE;
   }
#endif

   return TRUE;
}

/*
 * Habilita las salidas de potencia del A4988.
 *
 * ENABLE es activo en LOW: LOW = driver habilitado, HIGH = driver
 * deshabilitado.
 */
bool_t stepperEnable( void )
{
   if( !stepper.initialized ) {
      return FALSE;
   }

#if STEPPER_ENABLE_FIXED
   /*
    * Con el pin fijo no se escribe nada: si esta en GND el driver ya esta
    * habilitado; si esta en VCC no hay forma de habilitarlo.
    */
   #if STEPPER_ENABLE_FIXED_ENABLES
      stepper.enabled = TRUE;
      return TRUE;
   #else
      return FALSE;
   #endif
#else
   if( STEPPER_ENABLE_WRITE( FALSE ) == FALSE ) {
      return FALSE;
   }
   stepper.enabled = TRUE;

   return TRUE;
#endif
}

/*
 * Detiene primero el movimiento, si existe, y luego deshabilita el driver.
 *
 * Mantener estas dos acciones juntas evita dejar el motor generando pulsos
 * mientras ENABLE esta siendo desactivado.
 */
bool_t stepperDisable( void )
{
   if( !stepper.initialized ) {
      return FALSE;
   }

   /* Detener el tren de pulsos antes de deshabilitar las salidas. */
   stepperStop();

#if STEPPER_ENABLE_FIXED
   /*
    * Con el pin fijo no se escribe nada: si esta en VCC el driver ya
    * estaba deshabilitado; si esta en GND no hay forma de deshabilitarlo
    * (el movimiento si fue detenido).
    */
   #if STEPPER_ENABLE_FIXED_ENABLES
      return FALSE;
   #else
      stepper.enabled = FALSE;
      return TRUE;
   #endif
#else
   /* HIGH deshabilita las salidas del A4988. */
   if( STEPPER_ENABLE_WRITE( TRUE ) == FALSE ) {
      return FALSE;
   }
   stepper.enabled = FALSE;

   return TRUE;
#endif
}

/*
 * Configura el sentido de giro del motor.
 *
 * No se permite modificar DIR mientras STEP esta siendo generado porque el
 * A4988 toma el nuevo estado de DIR con el siguiente flanco ascendente.
 */
bool_t stepperSetDirection( stepperDirection_t direction )
{
   bool_t ok = FALSE;

   if( !stepper.initialized ) {
      return FALSE;
   }

   /* Evita aceptar valores que no pertenezcan al enum definido. */
   if( (direction != STEPPER_DIRECTION_FORWARD) &&
       (direction != STEPPER_DIRECTION_REVERSE) ) {
      return FALSE;
   }

#if STEPPER_DIR_FIXED
   /*
    * Con DIR fijo solo se acepta el sentido que ya impone el cableado.
    * No se escribe nada, por lo que tampoco importa si hay movimiento.
    */
   if( direction != STEPPER_DIR_FIXED_DIRECTION ) {
      return FALSE;
   }
   ok = TRUE;
#else
   /*
    * La comprobacion de movimiento y la escritura de DIR se hacen juntas
    * en una seccion critica para que stepperMove(), llamada desde otra
    * task, no pueda iniciar un movimiento entre ambas.
    */
   taskENTER_CRITICAL();
   if( stepper.state == STEPPER_STATE_IDLE ) {
      ok = ( STEPPER_DIR_WRITE( (direction == STEPPER_DIRECTION_REVERSE) ?
                                TRUE : FALSE ) != FALSE );
   }
   taskEXIT_CRITICAL();
#endif

   return ok;
}

/*
 * Configura la velocidad maxima (de crucero) del movimiento.
 *
 * La velocidad se almacena como frecuencia de pasos. El periodo de cada
 * pulso, incluida la rampa de aceleracion y de frenado, se calcula al
 * iniciar y durante el movimiento. De esta forma stepperSetSpeed()
 * solamente modifica la configuracion y no arranca ningun timer.
 *
 * Cualquier velocidad entre 1 y STEPPER_MAX_SPS es valida: la rampa es la
 * que permite al motor arrancar y llegar a ella sin perder pasos.
 */
bool_t stepperSetSpeed( uint32_t stepsPerSecond )
{
   bool_t ok = FALSE;

   if( !stepper.initialized ) {
      return FALSE;
   }

   /*
    * Se rechazan 0 (periodo infinito) y las velocidades cuyo periodo no
    * alcanza para los STEPPER_STEP_HIGH_US + STEPPER_STEP_LOW_US minimos.
    */
   if( (stepsPerSecond == 0) || (stepsPerSecond > STEPPER_MAX_SPS) ) {
      return FALSE;
   }

   /* La velocidad no se cambia mientras el timer esta generando STEP. */
   taskENTER_CRITICAL();
   if( stepper.state == STEPPER_STATE_IDLE ) {
      stepper.stepsPerSecond = stepsPerSecond;
      ok = TRUE;
   }
   taskEXIT_CRITICAL();

   return ok;
}

/*
 * Inicia un movimiento no bloqueante.
 *
 * La funcion solamente acepta el movimiento si:
 *   1. el driver esta inicializado,
 *   2. el A4988 esta habilitado,
 *   3. no existe otro movimiento en curso,
 *   4. se solicito al menos un pulso.
 *
 * IMPORTANTE 1: Se pierde un periodo desde la configuracion del timer
 * correspondiente porque recien se generaria el pulso pasado el tiempo colocado
 * al timer en Timer_Init(). Ese periodo es el de la velocidad inicial
 * (1 / STEPPER_START_SPS, 2,5 ms con los valores por defecto) y a el se suma
 * el tiempo que tarda la task en despertar y armar el timer.
 *
 * IMPORTANTE 2: esta funcion NO activa ENABLE. La habilitacion del driver es
 * responsabilidad de stepperEnable() / stepperInit().
 *
 * IMPORTANTE 3: esta funcion no arma el timer: acepta el movimiento y
 * notifica a la task del stepper, que es quien llama a Timer_Init().
 */
bool_t stepperMove( uint32_t steps )
{
   uint32_t targetSps;
   uint32_t startSps;
   bool_t accepted = FALSE;

   /* Un movimiento solo puede comenzar con el modulo listo y habilitado. */
   if( !stepper.initialized || !stepper.enabled || (steps == 0) ) {
      return FALSE;
   }

   /*
    * La comprobacion de movimiento en curso y la aceptacion del nuevo
    * movimiento se hacen en una sola seccion critica, para que dos tasks
    * no puedan aceptar movimientos a la vez.
    */
   taskENTER_CRITICAL();
   if( stepper.state == STEPPER_STATE_IDLE ) {

      /*
       * Perfil del movimiento. Las cuentas se hacen dentro de la seccion
       * critica porque son breves y asi stepperSetSpeed() no puede cambiar
       * la velocidad a mitad del calculo.
       *
       * Se parte de STEPPER_START_SPS, salvo que la velocidad configurada
       * sea menor: en ese caso no hay rampa y se va a velocidad constante.
       */
      targetSps = stepper.stepsPerSecond;
      startSps  = ( STEPPER_START_SPS < targetSps ) ? STEPPER_START_SPS
                                                    : targetSps;

      stepper.startTicks  = stepperSpsToTicks( startSps );
      stepper.targetTicks = stepperSpsToTicks( targetSps );
      stepper.periodTicks = stepper.startTicks;

      stepper.rampV0Sq = (float)startSps * (float)startSps;
      stepper.rampTwoA = 2.0f * (float)STEPPER_ACCEL_SPS2;
      stepper.rampTicksPerSecond = (float)Timer_microsecondsToTicks( 1000000UL );

      /*
       * Cantidad de pasos que hacen falta para pasar de la velocidad
       * inicial a la de crucero:
       *
       *       e = ( vcrucero^2 - vinicial^2 ) / ( 2 * a )
       *
       * La rampa de frenado necesita la misma cantidad.
       */
      if( targetSps > startSps ) {
         stepper.accelSteps = (uint32_t)
            ( ( (float)targetSps * (float)targetSps - stepper.rampV0Sq ) /
              stepper.rampTwoA );
      } else {
         stepper.accelSteps = 0;
      }

      /*
       * Guardamos la cantidad solicitada y marcamos el movimiento como
       * activo antes de notificar a la task, para que ella encuentre el
       * estado consistente.
       */
      stepper.totalSteps     = steps;
      stepper.remainingSteps = steps;
      stepper.state = STEPPER_STATE_STARTING;
      accepted = TRUE;
   }
   taskEXIT_CRITICAL();

   if( !accepted ) {
      return FALSE;
   }

   /* Despierta a la task para que arme el timer. */
   (void)xTaskNotify( stepper.taskHandle, STEPPER_NOTIFY_START, eSetBits );

   return TRUE;
}

/*
 * Detiene el movimiento actual.
 *
 * No modifica ENABLE: detener el motor y deshabilitar el driver son dos
 * acciones distintas y tienen sus propias funciones publicas.
 *
 * Puede llamarse en cualquier momento, incluso cuando no hay movimiento o
 * cuando este ya esta terminando por su cuenta.
 */
void stepperStop( void )
{
   bool_t notifyTask = FALSE;
   TickType_t ticksLeft;

   if( !stepper.initialized ) {
      return;
   }

   taskENTER_CRITICAL();

   switch( stepper.state ) {

   case STEPPER_STATE_STARTING:
      /*
       * La task todavia no armo el timer (o lo esta armando). Solo se marca
       * el movimiento como terminado; la task decide que liberar al
       * atender la notificacion END.
       */
      stepper.remainingSteps = 0;
      stepper.state = STEPPER_STATE_ENDING;
      notifyTask = TRUE;
      break;

   case STEPPER_STATE_RUNNING:
      /*
       * El timer esta armado: se silencia ahora mismo para que no haya
       * ni una interrupcion mas, sin esperar a que la task despierte.
       * La seccion critica evita que el Compare Match interrumpa esta
       * secuencia.
       */
      stepperQuiesceTimer();
      stepper.remainingSteps = 0;
      stepper.state = STEPPER_STATE_ENDING;
      notifyTask = TRUE;
      break;

   case STEPPER_STATE_ENDING:
      /* Ya esta terminando: la task libera el timer sin ayuda. */
   case STEPPER_STATE_IDLE:
   default:
      break;
   }

   /* STEP siempre debe quedar en LOW al detener el tren de pulsos. */
   (void)STEPPER_STEP_WRITE( FALSE );

   taskEXIT_CRITICAL();

   if( notifyTask ) {
      (void)xTaskNotify( stepper.taskHandle, STEPPER_NOTIFY_END, eSetBits );
   }

   /*
    * Espera, con un limite, a que la task libere el timer. Si la task es
    * mas prioritaria que quien llama, ya lo hizo al notificarla y este
    * lazo no se ejecuta ninguna vez.
    */
   ticksLeft = pdMS_TO_TICKS( STEPPER_STOP_TIMEOUT_MS );
   while( (stepper.state != STEPPER_STATE_IDLE) && (ticksLeft > 0) ) {
      vTaskDelay( 1 );
      ticksLeft--;
   }
}

/*
 * Informa si existe un movimiento en curso.
 *
 * Como la variable state puede ser modificada desde una interrupcion del
 * timer, esta consulta trabaja sobre el estado volatile almacenado dentro
 * de la estructura. Devuelve TRUE hasta que la task termino de liberar el
 * timer.
 */
bool_t stepperIsBusy( void )
{
   return ( stepper.state != STEPPER_STATE_IDLE ) ? TRUE : FALSE;
}

/*==================[end of file]============================================*/
