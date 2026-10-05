/*
 * stepper.c
 *
 * Driver sencillo para A4988 usando STEP/DIR y un timer sAPI.
 *
 * El A4988 se encarga internamente de:
 *   - secuenciar las corrientes de las dos fases,
 *   - hacer el microstepping,
 *   - limitar la corriente.
 *
 * La EDU-CIAA solo tiene que generar una se√±al STEP periodica y fijar DIR.
 */

#include <stddef.h>

#include "sapi_gpio.h"
#include "stepper.h"
#include "sapi_timer.h"
#include "event.h"

/*==================[estado interno]=========================================*/

static volatile uint32_t remainingSteps = 0;
static volatile bool_t moving = FALSE;

static uint32_t configuredStepsPerSecond = 1000;
static stepperDirection_t configuredDirection = STEPPER_DIRECTION_FORWARD;
static bool_t attached = FALSE;

/*==================[funciones auxiliares]===================================*/

static bool_t stepperSpeedToPeriodUs( uint32_t stepsPerSecond,
                                      uint32_t* periodUs )
{
   if( (stepsPerSecond == 0) || (periodUs == NULL) ) {
      return FALSE;
   }

   /*
    * Un periodo tiene que permitir al menos:
    *   - STEPPER_STEP_HIGH_US de HIGH
    *   - STEPPER_STEP_LOW_US de LOW
    *
    * Por eso no aceptamos periodos <= pulse+1us.
    */
   *periodUs = 1000000UL / stepsPerSecond;

   if( *periodUs <= (STEPPER_STEP_HIGH_US + STEPPER_STEP_LOW_US) ) {
      return FALSE;
   }

   return TRUE;
}

/*==================[callbacks del timer]===================================*/

/*
 * Compare Match 0:
 *   Se ejecuta una vez por periodo del timer.
 *   Aqui generamos el flanco ascendente de STEP.
 */
void stepperTimerCompareMatch0func( void* ptr )
{
   (void)ptr;

   if( !moving || (remainingSteps == 0) ) {
      return;
   }

   /* Flanco LOW->HIGH = un paso/microstep en el A4988. */
   gpioWrite( STEPPER_STEP_PIN, TRUE );

   remainingSteps--;
}

/*
 * Compare Match 1:
 *   Ocurre unos microsegundos despues de Match 0.
 *   Baja STEP para completar el pulso.
 */
void stepperTimerCompareMatch1func( void* ptr )
{
   (void)ptr;

   gpioWrite( STEPPER_STEP_PIN, FALSE );

   /*
    * Si este fue el ultimo pulso, ya no necesitamos mas interrupciones.
    * Se desinicializa TIMER1 y el movimiento queda terminado.
    */
   if( remainingSteps == 0 ) {
      Timer_DeInit( STEPPER_TIMER );
      moving = FALSE;
   }
}

/*==================[funciones publicas]====================================*/

void StepperTask(void * params){
   
   while ( TRUE ){
      xEventGroupWaitBits(getMainEventGroup(), DEAL_START, pdTRUE, pdTRUE, portMAX_DELAY);
      while ( TRUE ){ //Este while es hasta que se hayan repartido todas las cartas, de momento ni idea donde se calcula eso
         //rotarLoNecesario() ----> lo piensa mucci, Èl sabe que tiene que hacer
         //LO UNICO IMPORTANTE ES QUE VUELVA SOLO CUANDO SE HAYA TERMINADO DE ROTAR A ESA POSICION
         xEventGroupSetBits(getMainEventGroup(),EJECT_START);
         xEventGroupWaitBits(getMainEventGroup(),EJECT_DONE, pdTRUE, pdTRUE, portMAX_DELAY);
      }
      //rotarAlCentro()
      xEventGroupSetBits(getMainEventGroup(), DEAL_DONE);
   }

}

bool_t stepperInit( void )
{
   /* STEP, DIR y ENABLE son salidas digitales. */
   if( gpioConfig( STEPPER_STEP_PIN, GPIO_OUTPUT ) == FALSE ) {
      //return FALSE;
   }

   if( gpioConfig( STEPPER_DIR_PIN, GPIO_OUTPUT ) == FALSE ) {
      //return FALSE;
   }

   if( gpioConfig( STEPPER_ENABLE_PIN, GPIO_OUTPUT ) == FALSE ) {
      //return FALSE;
   }

   gpioWrite( STEPPER_STEP_PIN, FALSE );
   gpioWrite( STEPPER_DIR_PIN, FALSE );
   gpioWrite( STEPPER_ENABLE_PIN, TRUE );   /* A4988 deshabilitado */

   configuredDirection = STEPPER_DIRECTION_FORWARD;
   configuredStepsPerSecond = 1000;
   remainingSteps = 0;
   moving = FALSE;
   attached = TRUE;

   return TRUE;
}

bool_t stepperEnable( void )
{
   if( !attached ) {
      return FALSE;
   }

   /* ENABLE del A4988 es activo en LOW. */
   gpioWrite( STEPPER_ENABLE_PIN, FALSE );
   return TRUE;
}

bool_t stepperDisable( void )
{
   if( !attached ) {
      return FALSE;
   }

   stepperStop();
   gpioWrite( STEPPER_ENABLE_PIN, TRUE );
   return TRUE;
}

bool_t stepperSetDirection( stepperDirection_t direction )
{
   if( !attached || moving ) {
      return FALSE;
   }

   configuredDirection = direction;

   if( direction == STEPPER_DIRECTION_REVERSE ) {
      gpioWrite( STEPPER_DIR_PIN, TRUE );
   } else {
      gpioWrite( STEPPER_DIR_PIN, FALSE );
   }

   return TRUE;
}

bool_t stepperSetSpeed( uint32_t stepsPerSecond )
{
   uint32_t periodUs = 0;

   if( moving ) {
      return FALSE;
   }

   if( !stepperSpeedToPeriodUs( stepsPerSecond, &periodUs ) ) {
      return FALSE;
   }

   configuredStepsPerSecond = stepsPerSecond;
   return TRUE;
}

bool_t stepperMove( uint32_t steps )
{
   uint32_t periodUs = 0;

   if( !attached || moving || (steps == 0) ) {
      return FALSE;
   }

   if( !stepperSpeedToPeriodUs( configuredStepsPerSecond, &periodUs ) ) {
      return FALSE;
   }

   /* El driver debe estar habilitado para poder mover el motor. */
   gpioWrite( STEPPER_ENABLE_PIN, FALSE );

   gpioWrite( STEPPER_STEP_PIN, FALSE );

   remainingSteps = steps;
   moving = TRUE;

   /*
    * TIMER1 genera un evento cada periodo STEP.
    * Match 1 se usa para terminar el pulso 2 us despues.
    */
   Timer_Init( STEPPER_TIMER,
               Timer_microsecondsToTicks( periodUs ),
               stepperTimerCompareMatch0func );

   Timer_EnableCompareMatch( STEPPER_TIMER,
                              TIMERCOMPAREMATCH1,
                              Timer_microsecondsToTicks( STEPPER_STEP_HIGH_US ),
                              stepperTimerCompareMatch1func );

   return TRUE;
}

void stepperStop( void )
{
   if( !moving ) {
      gpioWrite( STEPPER_STEP_PIN, FALSE );
      return;
   }

   moving = FALSE;
   remainingSteps = 0;

   gpioWrite( STEPPER_STEP_PIN, FALSE );

   /* El compare match 1 deja de ser necesario. */
   Timer_DisableCompareMatch( STEPPER_TIMER, TIMERCOMPAREMATCH1 );
   Timer_DeInit( STEPPER_TIMER );
}

bool_t stepperIsBusy( void )
{
   return moving;
}
