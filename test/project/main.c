#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "components/motor/inc/shuffler.h"
#include "components/error/inc/tanguito_error.h"

void mainTask( void* pvParameters )
{
   while( TRUE ) {
      gpioToggle(CIAA_BOARD_LED);
      vTaskDelay( pdMS_TO_TICKS( 100 ) );
   }
}

int main( void )
{
   boardConfig();
   
   // hacemos el init del shuffler, pero más fachero porque pregunta por errores
   error_t err = shufflerInit();
   if( !tanguitoErrorIsOk( err ) ) {
      gpioWrite( LEDR, ON );   // señalizamos el fallo
      while( TRUE );           // no arrancamos el scheduler
   }  

   xTaskCreate(
      mainTask,           // Función de la tarea
      "mainTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Parámetros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );

   vTaskStartScheduler();

   while( TRUE );
   return 0;
}
