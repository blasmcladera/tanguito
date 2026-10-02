#include "shuffler.h"
#include "tanguito_error.h"
#include "event.h"

#include "FreeRTOS.h"
#include "task.h"

void ShufflerTask(void * params){
   while(1){
      xEventGroupWaitBits(getMainEventGroup(),SHUFFLE_START, pdTRUE, pdTRUE, portMAX_DELAY);
      while ( 1 ) {//aca habria que ver si usar un contador o un timer ni idea
         //random(izq o der)
         //prender()
         vTaskDelay(pdMS_TO_TICKS(2));//tiempo suficiente para que caiga la carta
         //apagar
      }
      xEventGroupSetBits(getMainEventGroup(), SHUFFLE_DONE);
   }
}

error_t motorShufflerInit(void* param)
{
    /*
     * Inicializa el GPIO utilizado para controlar el STBY
     * y configura el PWM que controla la velocidad del motor.
     * Deja el motor detenido y el driver deshabilitado
     * en un estado inicial seguro.
     */
    return TANGUITO_OK;
}

error_t motorShufflerEnable(shuffle_motor_t motor)
{
    /*
     * Habilita el TB6612FNG mediante su entrada STBY,
     * permitiendo que el driver controle el motor.
     */
   
   xTaskCreate(ShufflerTask,"ShufflerTask",255,NULL,0,NULL);
   
   return TANGUITO_OK;
}

error_t motorShufflerDisable(shuffle_motor_t motor)
{
    /*
     * Deshabilita el TB6612FNG mediante su entrada STBY
     * y detiene el motor colocando el PWM en 0%.
     */
    return TANGUITO_OK;
}

error_t motorShufflerSetSpeed(uint8_t speed, shuffle_motor_t motor)
{
    /*
     * Configura la velocidad del motor mediante PWM.
     *
     * speed representa el duty cycle del PWM:
     * 0   -> 0%
     * 50  -> 50%
     * 100 -> 100%
     */
    return TANGUITO_OK;
}

error_t motorShufflerStart(shuffle_motor_t motor)
{
    /*
     * Inicia el movimiento del motor utilizando
     * la velocidad configurada previamente mediante
     * motorShufflerSetSpeed().
     */
    return TANGUITO_OK;
}

error_t motorShufflerStop(shuffle_motor_t motor)
{
    /*
     * Detiene el motor colocando el duty cycle del PWM
     * en 0%, sin deshabilitar necesariamente el driver.
     */
    return TANGUITO_OK;
}
