#ifndef _MEF_H_
#define _MEF_H_

#include "error.h"

typedef enum {
   MEF_OFF,
   MEF_MENU,
   MEF_WAIT,
   MEF_LEFT,
   MEF_RIGHT,
   MEF_ROTATE,
   MEF_EJECT   
} state_t;

error_t mefInit();
error_t mefUpdate();

#endif