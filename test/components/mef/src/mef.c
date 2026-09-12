#include buttons.h

static int state;

void mefInit (void* param){



}

void mefUpdate(void* param){

    switch (state){
        case MENU:
            //SE APRETO EL BOTON DE START
            if (flagButton1Pressed){
                flagButton1Pressed = FALSE;
                state = ESPERA;
            }
            break;
        case ESPERA:
            //Este flag habria que ver que onda
            if (!flagShuffleTerminado){
                delay();//Habria que ver que delay hace falta para que las cartas caigan bien
                state = random(LEFT, RIGHT);
            } else {
                state = ROTAR;
            }
            break;
        case LEFT:
            driverMotorIzquierdo(ON); //Cuando los apagamos? revisar tema del delay cpaz va acá
            state = ESPERA;
            break;
        case RIGHT:
            driverMotorDerecho(ON);
            state = ESPERA;
            break;
        case ROTAR:
            //Este flag habria que ver que onda
            if (!flagDealTerminado){
                rotar(x);
                state = EYECTAR;
            } else {
                rotar(centro);
                state = MENU;
            }
            break;
        case EYECTAR:
            driverMotorDealer(ON);
            state = ROTAR;
            break;
        default: break;
    }

}