#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shuffler.h"

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
   if( shufflerInit() != ERR_OK ) {
      // Falló la inicialización: señalizamos y no arrancamos el scheduler
      gpioWrite( LEDR, ON );  // damos algún tipo de señal de que hubo error
      while( TRUE );          // nos quedamos acá y no seguimos ejecutando
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
