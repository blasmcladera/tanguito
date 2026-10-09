#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "stepper.h"

void MainTask( void* pvParameters )
{
   stepperSetSpeed(5000);
   while( TRUE ) {
      stepperMove(2000);
      //gpioToggle(CIAA_BOARD_LED);
      //vTaskDelay( pdMS_TO_TICKS( 1000 ) );
   }
}

int main( void )
{
   boardConfig();
   stepperInit();
   
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
