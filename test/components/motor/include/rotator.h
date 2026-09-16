#ifndef _ROTATOR_H_
#define _ROTATOR_H_

#include "error.h"

//Esto es teniendo en cuenta el driver A4988
#define MOTOR_ROTATOR_STEP_PIN NIIDEA1
#define MOTOR_ROTATOR_DIR_PIN NIIDEA2
#define MOTOR_ROTATOR_ENABLE_PIN NIIDEA3
#define MOTOR_ROTATOR_DIR_CW   0
#define MOTOR_ROTATOR_DIR_CCW  1

#define MOTOR_ROTATOR_STEPS_PER_REV    NIIDEAPARAQUESIRVE

error_t motorRotatorInit(void* param);
error_t motorRotatorEnable(void* param);
error_t motorRotatorDisable(void* param);
error_t motorRotatorSetDirection(uint8_t direction);
error_t motorRotatorStep(void* param);
error_t motorRotatorMove(uint32_t steps);

#endif