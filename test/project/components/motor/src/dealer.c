#include "dealer.h"
#include "tanguito_error.h"
#include "event.h"

#include "FreeRTOS.h"
#include "task.h"

void DealerTask(void * params){

   while (1){
      
      xEventGroupWaitBits(getMainEventGroup(),EJECT_START, pdTRUE, pdTRUE, portMAX_DELAY);
      //prender()
      vTaskDelay(pdMS_TO_TICKS(2));//tiempo suficiente para que caiga la carta
      //apagar
      xEventGroupSetBits(getMainEventGroup(),EJECT_DONE);
   }

}

void dealerInit(){
   
   
   xTaskCreate(
      DealerTask,           // Función de la tarea
      "DealerTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Parámetros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );
   
}