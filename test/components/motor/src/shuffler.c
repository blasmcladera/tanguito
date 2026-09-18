#include "shuffler.h"
#include "error.h"

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