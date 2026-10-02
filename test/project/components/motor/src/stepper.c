/*
 * stepper.c
 *
 * Driver sencillo para un motor paso a paso bipolar controlado mediante un
 * A4988 usando STEP/DIR y un timer de la sAPI.
 *
 * El A4988 se ocupa internamente de la conmutacion de las bobinas, del
 * microstepping y de la regulacion de corriente. La EDU-CIAA solamente
 * genera el tren de pulsos STEP y mantiene configurado DIR/ENABLE.
 */

/*==================[inclusions]=============================================*/

#include <stddef.h>

#include "sapi_gpio.h"
#include "sapi_timer.h"
#include "stepper.h"

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
   /* moving         */ FALSE,
   /* stepsPerSecond */ 1000,
   /* direction      */ STEPPER_DIRECTION_FORWARD,
   /* initialized    */ FALSE,
   /* enabled        */ FALSE
};

/*==================[funciones auxiliares]===================================*/

/*
 * Convierte una velocidad expresada en pulsos por segundo a un periodo
 * expresado en microsegundos.
 *
 * Ademas de realizar la conversion, comprueba que el periodo sea suficiente
 * para respetar los tiempos minimos seleccionados para HIGH y LOW.
 */
static bool_t stepperSpeedToPeriodUs( uint32_t stepsPerSecond,
                                      uint32_t* periodUs )
{
   if( (stepsPerSecond == 0) || (periodUs == NULL) ) {
      return FALSE;
   }

   /*
    * Para una velocidad f de pulsos por segundo:
    *
    *       T = 1 / f
    *
    * Como el timer trabaja en microsegundos, usamos 1.000.000 us por
    * segundo y obtenemos el periodo entero en microsegundos.
    */
   *periodUs = 1000000UL / stepsPerSecond;

   /*
    * Cada periodo debe contener como minimo:
    *
    *       HIGH = STEPPER_STEP_HIGH_US
    *       LOW  = STEPPER_STEP_LOW_US
    *
    * Se permite exactamente la suma de ambos tiempos, porque ese es el
    * periodo minimo valido para esta implementacion.
    */
   if( *periodUs < (STEPPER_STEP_HIGH_US + STEPPER_STEP_LOW_US) ) {
      return FALSE;
   }

   return TRUE;
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
 */
void stepperTimerCompareMatch0func( void* ptr )
{
   /* La sAPI no utiliza el argumento del callback en este modulo. */
   (void)ptr;

   /*
    * Si el movimiento ya termino o fue cancelado, no debe generarse otro
    * pulso.
    */
   if( !stepper.moving || (stepper.remainingSteps == 0) ) {
      return;
   }

   /* Genera el flanco ascendente que ordena un paso al A4988. */
   gpioWrite( stepper.stepPin, TRUE );

   /* Este flanco ya representa uno de los pulsos solicitados. */
   stepper.remainingSteps--;
}

/*
 * Compare Match 1:
 *
 * Se programa a STEPPER_STEP_HIGH_US desde el comienzo de cada periodo.
 * Su unica funcion durante un movimiento normal es bajar STEP para que el
 * ancho del pulso HIGH sea exactamente el tiempo configurado.
 */
void stepperTimerCompareMatch1func( void* ptr )
{
   /* La sAPI no utiliza el argumento del callback en este modulo. */
   (void)ptr;

   /* Termina el pulso STEP actual. */
   gpioWrite( stepper.stepPin, FALSE );

   /*
    * remainingSteps llega a cero despues del ultimo flanco ascendente.
    * El ultimo pulso, sin embargo, todavia debe completar sus
    * STEPPER_STEP_HIGH_US antes de finalizar el movimiento.
    */
   if( stepper.remainingSteps == 0 ) {
      /* El Compare Match 1 ya no es necesario para otro movimiento. */
      Timer_DisableCompareMatch( stepper.timer,
                                 TIMERCOMPAREMATCH1 );

      /* Libera el timer y desactiva el estado de movimiento. */
      Timer_DeInit( stepper.timer );
      stepper.moving = FALSE;
   }
}

/*==================[funciones publicas]====================================*/

/*
 * Inicializa el modulo STEP/DIR/ENABLE del A4988.
 *
 * No se genera ningun pulso STEP durante esta operacion.
 */
bool_t stepperInit( void )
{
   /*
    * STEP, DIR y ENABLE son salidas digitales porque los tres pines son
    * controlados por la EDU-CIAA hacia entradas del A4988.
    */
   if( gpioConfig( stepper.stepPin, GPIO_OUTPUT ) == FALSE ) {
      return FALSE;
   }

   if( gpioConfig( stepper.dirPin, GPIO_OUTPUT ) == FALSE ) {
      return FALSE;
   }

   if( gpioConfig( stepper.enablePin, GPIO_OUTPUT ) == FALSE ) {
      return FALSE;
   }

   /*
    * STEP debe comenzar en LOW para evitar generar accidentalmente un
    * flanco ascendente al inicializar el modulo.
    */
   gpioWrite( stepper.stepPin, FALSE );

   /* El sentido por defecto es FORWARD. */
   stepper.direction = STEPPER_DIRECTION_FORWARD;
   gpioWrite( stepper.dirPin, FALSE );

   /*
    * Marcamos el modulo como inicializado antes de llamar a
    * stepperEnable(), ya que esa funcion comprueba este estado.
    */
   stepper.initialized = TRUE;

   /*
    * La velocidad inicial es de 1000 pulsos/s y no hay movimiento activo.
    */
   stepper.stepsPerSecond = 1000;
   stepper.remainingSteps = 0;

   /*
    * Se habilita el A4988.
    */
   if( !stepperEnable() ) {
      stepper.initialized = FALSE;
      return FALSE;
   }

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

   gpioWrite( stepper.enablePin, FALSE );
   stepper.enabled = TRUE;

   return TRUE;
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

   /* HIGH deshabilita las salidas del A4988. */
   gpioWrite( stepper.enablePin, TRUE );
   stepper.enabled = FALSE;

   return TRUE;
}

/*
 * Configura el sentido de giro del motor.
 *
 * No se permite modificar DIR mientras STEP esta siendo generado porque el
 * A4988 toma el nuevo estado de DIR con el siguiente flanco ascendente.
 */
bool_t stepperSetDirection( stepperDirection_t direction )
{
   if( !stepper.initialized || stepper.moving ) {
      return FALSE;
   }

   /* Evita aceptar valores que no pertenezcan al enum definido. */
   if( (direction != STEPPER_DIRECTION_FORWARD) &&
       (direction != STEPPER_DIRECTION_REVERSE) ) {
      return FALSE;
   }

   stepper.direction = direction;

   if( direction == STEPPER_DIRECTION_REVERSE ) {
      gpioWrite( stepper.dirPin, TRUE );
   } else {
      gpioWrite( stepper.dirPin, FALSE );
   }

   return TRUE;
}

/*
 * Configura la velocidad de movimiento.
 *
 * La velocidad se almacena como frecuencia de pasos y el periodo de STEP
 * se calcula al iniciar el movimiento. De esta forma stepperSetSpeed()
 * solamente modifica la configuracion y no arranca ningun timer.
 */
bool_t stepperSetSpeed( uint32_t stepsPerSecond )
{
   uint32_t periodUs = 0;

   /* La velocidad no se cambia mientras el timer esta generando STEP. */
   if( !stepper.initialized || stepper.moving ) {
      return FALSE;
   }

   /* Reutilizamos la validacion que tambien se usa al iniciar el movimiento. */
   if( !stepperSpeedToPeriodUs( stepsPerSecond, &periodUs ) ) {
      return FALSE;
   }

   /* periodUs solo se utiliza para validar que la frecuencia sea posible. */
   (void)periodUs;

   stepper.stepsPerSecond = stepsPerSecond;

   return TRUE;
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
 * IMPORTANTE 1: Se pierde un periodo desde la configiración del timer 
 * correspondiente porque recien se generaría el puslo pasado el tiempo colocado 
 * al timer en Timer_Init().
 *
 * IMPORTANTE 2: esta funcion NO activa ENABLE. La habilitacion del driver es
 * responsabilidad de stepperEnable() / stepperInit().
 */
bool_t stepperMove( uint32_t steps )
{
   uint32_t periodUs = 0;

   /* Un movimiento solo puede comenzar con el modulo listo y habilitado. */
   if( !stepper.initialized || !stepper.enabled ||
       stepper.moving || (steps == 0) ) {
      return FALSE;
   }

   /* Obtiene el periodo correspondiente a la velocidad configurada. */
   if( !stepperSpeedToPeriodUs( stepper.stepsPerSecond, &periodUs ) ) {
      return FALSE;
   }

   /* STEP debe empezar en LOW antes de comenzar un nuevo tren de pulsos. */
   gpioWrite( stepper.stepPin, FALSE );

   /*
    * Guardamos la cantidad solicitada y marcamos el movimiento como activo
    * antes de habilitar el timer, para que el primer callback encuentre el
    * estado consistente.
    */
   stepper.remainingSteps = steps;
   stepper.moving = TRUE;

   /*
    * Compare Match 0 define el periodo del tren STEP.
    *
    * Al llegar a Match 0:
    *       STEP -> HIGH
    *       timer counter -> 0
    *
    * Por lo tanto, el siguiente periodo vuelve a comenzar despues de
    * 'periodUs' microsegundos.
    */
   Timer_Init( stepper.timer,
               Timer_microsecondsToTicks( periodUs ),
               stepperTimerCompareMatch0func );

   /*
    * Compare Match 1 ocurre STEPPER_STEP_HIGH_US despues de cada Match 0.
    * Su callback baja STEP y, por lo tanto, fija el ancho HIGH del pulso.
    */
   Timer_EnableCompareMatch( stepper.timer,
                              TIMERCOMPAREMATCH1,
                              Timer_microsecondsToTicks(STEPPER_STEP_HIGH_US),
                              stepperTimerCompareMatch1func );

   return TRUE;
}

/*
 * Detiene el movimiento actual.
 *
 * No modifica ENABLE: detener el motor y deshabilitar el driver son dos
 * acciones distintas y tienen sus propias funciones publicas.
 */
void stepperStop( void )
{
   /* STEP siempre debe quedar en LOW al detener el tren de pulsos. */
   gpioWrite( stepper.stepPin, FALSE );

   /* Si no habia movimiento, no hay ningun timer que liberar. */
   if( !stepper.moving ) {
      return;
   }

   /* Cancelamos los pulsos pendientes. */
   stepper.moving = FALSE;
   stepper.remainingSteps = 0;

   /* El Compare Match 1 deja de ser necesario. */
   Timer_DisableCompareMatch( stepper.timer,
                              TIMERCOMPAREMATCH1 );

   /* Se detiene y libera el timer utilizado para generar STEP. */
   Timer_DeInit( stepper.timer );
}

/*
 * Informa si existe un movimiento en curso.
 *
 * Como la variable moving puede ser modificada desde una interrupcion del
 * timer, esta consulta trabaja sobre el estado volatile almacenado dentro
 * de la estructura.
 */
bool_t stepperIsBusy( void )
{
   return stepper.moving;
}

/*
 * Gira el engranaje de salida "Degrees" grados (positivo = FORWARD,
 * negativo = REVERSE). Es no bloqueante: usar stepperIsBusy() para saber
 * cuando termino.
 */
bool_t turnDegrees ( float Degrees){
   const float degreesPerStep = STEPPER_STEP_ANGLE_DEG / STEPPER_MICROSTEPS; //Valor por si cambiamos la cantidad de grados por step
   float stepsF;
   int32_t stepsRounded;
   uint32_t steps;
   stepperDirection_t dir;

   if( !stepper.attached || stepper.moving ) {
      return FALSE;
   }

   /* Grados en la salida a grados en el eje del motor a pasos */
   stepsF = (Degrees * STEPPER_GEAR_RATIO) / degreesPerStep + stepResidue;

   /* Redondeo al entero mas cercano (con signo) */
   stepsRounded = (int32_t)( (stepsF >= 0.0f) ? (stepsF + 0.5f)
                                              : (stepsF - 0.5f) );

   if( stepsRounded == 0 ) {
      stepResidue = stepsF;   /* movimiento menor a un paso: lo acumulo */
      return TRUE;
   }

   if( stepsRounded < 0 ) {
      stepper.dir = STEPPER_DIRECTION_REVERSE;
      steps = (uint32_t)(-stepsRounded);
   } else {
      stepper.dir = STEPPER_DIRECTION_FORWARD;
      steps = (uint32_t)stepsRounded;
   }

   if( !stepperSetDirection( stepper.dir ) ) {
      return FALSE;
   }

   if( !stepperMove( steps ) ) {
      return FALSE;
   }

   stepResidue = stepsF - (float)stepsRounded;
   return TRUE;

}

/*==================[end of file]============================================*/
