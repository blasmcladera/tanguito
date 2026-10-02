#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"

void MainTask( void* pvParameters )
{
   while( TRUE ) {
      gpioToggle(CIAA_BOARD_LED);
      vTaskDelay( pdMS_TO_TICKS( 100 ) );
   }
}

int main( void )
{
   boardConfig();

   xTaskCreate(
      MainTask,           // Funci�n de la tarea
      "MainTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Par�metros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );

   vTaskStartScheduler();


   while( TRUE );
   return 0;
}
