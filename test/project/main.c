#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "shuffler.h"
#include "tanguito_error.h"
#include "lcd.h"
#include "button.h"
#include "event.h"
#include "configuration.h"

void MainTask( void* pvParameters )
{
   /*
   while( TRUE ) {
      gpioToggle(CIAA_BOARD_LED);
      vTaskDelay( pdMS_TO_TICKS( 100 ) );
   }
   */
   
   while( TRUE ){
      xEventGroupSetBits(getMainEventGroup(), CONFIGURATION_START);
      xEventGroupWaitBits(getMainEventGroup(),CONFIGURATION_DONE, pdTRUE, pdTRUE, portMAX_DELAY);
      xEventGroupSetBits(getMainEventGroup(), SHUFFLE_START);
      xEventGroupWaitBits(getMainEventGroup(),SHUFFLE_DONE, pdTRUE, pdTRUE, portMAX_DELAY);
      xEventGroupSetBits(getMainEventGroup(), DEAL_START);
      xEventGroupWaitBits(getMainEventGroup(),DEAL_DONE, pdTRUE, pdTRUE, portMAX_DELAY);
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

   buttonsInit();
   eventInit();
   i2cLcdInit();                      // inicia I2C0 y crea la tarea del LCD
   configInit();
   
   xTaskCreate(
      MainTask,           // Función de la tarea
      "MainTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Parámetros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );
   
   //En el main se hacen los inits de cada modulo para que estos creen las tasks

   vTaskStartScheduler();

   while( TRUE );
   return 0;
}
