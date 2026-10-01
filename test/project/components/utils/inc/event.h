#ifndef _EVENT_H_
#define _EVENT_H_

#include "FreeRTOS.h"
#include "event_groups.h"

#define SHUFFLE_START         (1 << 0)
#define SHUFFLE_DONE          (1 << 1)

#define EJECT_START           (1 << 2)
#define EJECT_DONE            (1 << 3)

#define DEAL_START            (1 << 4)
#define DEAL_DONE             (1 << 5)

#define CONFIGURATION_START   (1 << 6)
#define CONFIGURATION_DONE    (1 << 7)


EventGroupHandle_t getMainEventGroup();
#endif