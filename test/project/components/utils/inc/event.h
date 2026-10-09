#ifndef _EVENT_H_
#define _EVENT_H_

#include "FreeRTOS.h"
#include "event_groups.h"

#define SHUFFLE_START         (1 << 0)
#define SHUFFLE_DONE          (1 << 1)

#define DEAL_START            (1 << 2)
#define DEAL_DONE             (1 << 3)

#define CONFIGURATION_START   (1 << 4)
#define CONFIGURATION_DONE    (1 << 5)


#define EJECT_START           (1 << 0)
#define EJECT_DONE            (1 << 1)

#define MOVE_DONE             (1 << 2)

#define PRESSED_ENTER         (1 << 0)
#define PRESSED_UP            (1 << 1)
#define PRESSED_DOWN          (1 << 2)
#define PRESSED_BACK          (1 << 3)
#define PRESSED_BUTTON        (PRESSED_ENTER | PRESSED_UP | PRESSED_DOWN | PRESSED_BACK)


EventGroupHandle_t getMainEventGroup();
EventGroupHandle_t getMovementEventGroup();
EventGroupHandle_t getButtonEventGroup();
uint8_t isEventAndClear(EventBits_t);
void eventInit();
#endif
