#ifndef _SHUFFLER_H_
#define _SHUFFLER_H_

#include <stdint.h>
#include "error.h"

// Esto sería para controlar 1 solo motor, podríamos hacer que cada función de las siguientes
// reciba un enum y haga lo que tiene que hacer sobre el motor recibido por el enum

typedef enum {
   SHUFFLE_LEFT,
   SHUFFLE_RIGHT,
} shuffle_motor_t;   

// TB6612FNG
#define MOTOR_SHUFFLER_PWM_LEFT_PIN       NIIDEA1
#define MOTOR_SHUFFLER_PWM_RIGHT_PIN       NIIDEA2
// creo que el L298N no usa esto, habria que averiguar
#define MOTOR_SHUFFLER_STBY_PIN      NIIDEA3

// Dirección fija del motor.
// AIN1/AIN2 se pueden dejar conectados directamente a VCC/GND.
// Estas entradas determinan dirección, lo que dice es que para ahorrarnos pines podemos dejarlas seteadas directmanete
// conectandolas a VCC o GND para que giren en un solo sentido 

#define MOTOR_SHUFFLER_MIN_SPEED     0
#define MOTOR_SHUFFLER_MAX_SPEED     100

error_t motorShufflerInit(void* param);

error_t motorShufflerEnable(shuffle_motor_t motor);
error_t motorShufflerDisable(shuffle_motor_t motor);

error_t motorShufflerSetSpeed(uint8_t speed, shuffle_motor_t motor);

error_t motorShufflerStart(shuffle_motor_t motor);
error_t motorShufflerStop(shuffle_motor_t motor);

#endif