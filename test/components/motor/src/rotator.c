#include "rotator.h"
#include "error.h"

error_t motorRotatorInit(void* param)
{
    /*
     * Inicializa los GPIO utilizados para controlar el A4988:
     * STEP, DIR y ENABLE. Deja el motor en un estado inicial seguro.
     */
     return TANGUITO_OK;
}

error_t motorRotatorEnable(void* param)
{
    /*
     * Habilita el A4988 mediante su entrada ENABLE,
     * permitiendo que el driver entregue corriente al motor.
     */
     return TANGUITO_OK;
}

error_t motorRotatorDisable(void* param)
{
    /*
     * Deshabilita el A4988 mediante su entrada ENABLE,
     * evitando que el driver entregue corriente al motor.
     */
     return TANGUITO_OK;
}

error_t motorRotatorSetDirection(uint8_t direction)
{
    /*
     * Configura el sentido de giro del motor mediante
     * la entrada DIR del A4988.
     */
     return TANGUITO_OK;
}

error_t motorRotatorStep(void* param)
{
    /*
     * Genera un pulso en la entrada STEP del A4988.
     * Cada pulso hace que el driver avance el motor un paso.
     */
     return TANGUITO_OK;
}

error_t motorRotatorMove(uint32_t steps)
{
    /*
     * Mueve el motor la cantidad de pasos indicada,
     * generando sucesivamente los pulsos STEP necesarios.
     */
     return TANGUITO_OK;
}