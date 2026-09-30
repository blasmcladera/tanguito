#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"

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
