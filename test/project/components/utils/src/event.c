#include "event.h"

EventGroupHandle_t mainEventGroup = NULL;

EventGroupHandle_t getMainEventGroup(){
   return mainEventGroup;
}