#include "event.h"
#include "FreeRTOS.h"
#include "task.h"

void ConfigurationTask(void * params){
   
while (1){
   xEventGroupWaitBits(getMainEventGroup(),CONFIGURATION_START, pdTRUE, pdTRUE, portMAX_DELAY);
   do {
      EventBits_t event = xEventGroupWaitBits(getButtonEventGroup(),PRESSED_BUTTON, pdFALSE, pdFALSE, portMAX_DELAY);
      switch (isEvent(event)){
         case PRESSED_ENTER:
            break;
        
         case PRESSED_UP:
            break;
              
         case PRESSED_DOWN:
            break;
              
         case PRESSED_BACK:
            break;
        
         default:
            break;
      }
   } while ()
      
}
}