#ifndef _EVENT_H_
#define _EVENT_H_

#include "FreeRTOS.h"
#include "event_groups.h"

#define SHUFFLER_START  (1<<0)
#define SHUFFLER_DONE   (1<<1)
#define ROTATOR_START   (1<<2)
#define ROTATOR_DONE    (1<<3)
#define DEALER_START    (1<<4)
#define DEALER_DONE     (1<<5)

EventGroupHandle_t getMainGroupEvent();
#endif