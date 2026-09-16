#include "button.h"
#include "mef.h"
#include "error.h"

static state_t state;


error_t mefInit (){


   return TANGUITO_OK;
}

error_t mefUpdate(){

    switch (state){
        case MEF_OFF:
           
           break;
        case MEF_MENU:
            //SE APRETO EL BOTON DE START
            if (flagButton1Pressed){
                flagButton1Pressed = FALSE;
                state = MEF_WAIT;
            }
            break;
        case MEF_WAIT:
            //Este flag habria que ver que onda
            driverMotorIzquierdo(OFF);
            driverMotorDerecho(OFF);
            if (!flagShuffleTerminado){
                delay();//Habria que ver que delay hace falta para que las cartas caigan bien
                state = random(MEF_LEFT, MEF_RIGHT);
            } else {
                state = MEF_ROTATE;
            }
            break;
        case MEF_LEFT:
            driverMotorIzquierdo(ON); //Cuando los apagamos? revisar tema del delay cpaz va acÃ¡
            state = MEF_WAIT;
            break;
        case MEF_RIGHT:
            driverMotorDerecho(ON);
            state = MEF_WAIT;
            break;
        case MEF_ROTATE:
            //Este flag habria que ver que onda
            driverMotorDealer(OFF);
            if (!flagDealTerminado){
                rotar(x);
                state = MEF_EJECT;
            } else {
                rotar(centro);
                state = MEF_MENU;
            }
            break;
        case MEF_EJECT:
            driverMotorDealer(ON);
            state = MEF_ROTATE;
            break;
        default: break;
    }
    return TANGUITO_OK;

}
