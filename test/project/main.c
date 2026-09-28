//#include "mef.h"
#include "stepper.h"
//#include "event.h"
#include "sapi.h"
#include "sapi_gpio.h"

int main(void)
{
    boardConfig();

    stepperInit();
   delay(1);
    while (1) {
        gpioWrite( STEPPER_STEP_PIN, FALSE );
         delay(1000);
       gpioWrite( STEPPER_STEP_PIN, TRUE );
         delay(1000);
        //stepperMove(200);
        //delay(1000);
    }
}