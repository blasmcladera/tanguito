#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
<<<<<<< HEAD
#include "components/motor/inc/shuffler.h"
#include "components/error/inc/tanguito_error.h"
=======
#include "stepper.h"
>>>>>>> d1907a8419b87e65d89ff5d79f97c0b27ff1a458

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
<<<<<<< HEAD
   
   // hacemos el init del shuffler, pero más fachero porque pregunta por errores
   error_t err = shufflerInit();
   if( !tanguitoErrorIsOk( err ) ) {
      gpioWrite( LEDR, ON );   // señalizamos el fallo
      while( TRUE );           // no arrancamos el scheduler
   }  

=======
   stepperInit();
   
>>>>>>> d1907a8419b87e65d89ff5d79f97c0b27ff1a458
   xTaskCreate(
      MainTask,           // Funciï¿½n de la tarea
      "MainTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Parï¿½metros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );

   vTaskStartScheduler();

   while( TRUE );
   return 0;
}
