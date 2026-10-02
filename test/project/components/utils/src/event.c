#include "event.h"

static EventGroupHandle_t mainEventGroup = NULL;
static EventGroupHandle_t buttonEventGroup = NULL;

EventGroupHandle_t getMainEventGroup()    {return mainEventGroup;}

EventGroupHandle_t getButtonEventGroup()  {return buttonEventGroup;}

void eventInit(){
   mainEventGroup = xEventGroupCreate();
   buttonEventGroup = xEventGroupCreate();
   if ((mainEventGroup == NULL) | (buttonEventGroup == NULL)){
      //ERROR
   }
   
}

uint8_t isEvent(EventBits_t event){
   if ((event & PRESSED_ENTER) != 0) {
      xEventGroupClearBits(buttonEventGroup,PRESSED_ENTER);
      return PRESSED_ENTER;
   }
   if ((event & PRESSED_UP) != 0) {
      xEventGroupClearBits(buttonEventGroup,PRESSED_UP);
      return PRESSED_UP;
   }
   if ((event & PRESSED_DOWN) != 0) {
      xEventGroupClearBits(buttonEventGroup,PRESSED_DOWN);
      return PRESSED_DOWN;
   }
   if ((event & PRESSED_BACK) != 0) {
      xEventGroupClearBits(buttonEventGroup,PRESSED_BACK);
      return PRESSED_BACK;
   }
   return 0;//ERROR
}