#ifndef _SHUFFLER_H_
#define _SHUFFLER_H_

#include <stdint.h>
#include "tanguito_error.h"

/*
 * Driver de los 2 motores DC del shuffler con L298N (2 puentes H).
 *
 * Velocidad: PWM en los pines ENABLE del L298N (ENA / ENB).
 *
 * Dirección: fija. IN1 e IN4 en 1 (por GPIO), IN2 e IN3 conectados a GND.
 *   -> Así los motores giran siempre en un solo sentido y ahorramos pines.
 */

typedef enum {
   SHUFFLE_LEFT,    // Motor 1: ENA <- T_FIL1 (PWM0)
   SHUFFLE_RIGHT,   // Motor 2: ENB <- T_FIL2 (PWM3)
   SHUFFLE_MOTOR_COUNT
} shuffle_motor_t;

// Pines de PWM (enables del L298N)
#define MOTOR_SHUFFLER_PWM_LEFT_PIN      PWM0   // T_FIL1 -> ENA
#define MOTOR_SHUFFLER_PWM_RIGHT_PIN     PWM3   // T_FIL2 -> ENB

// Pines de dirección (siempre en 1). IN2 e IN3 van a GND por hardware.
#define MOTOR_SHUFFLER_IN1_PIN           GPIO0
#define MOTOR_SHUFFLER_IN4_PIN           GPIO3

// Velocidad en porcentaje de potencia
#define MOTOR_SHUFFLER_MIN_SPEED         0
#define MOTOR_SHUFFLER_MAX_SPEED         100

// Cantidad de veces que se prende cada motor del shuffler (aleatoria dentro de este rango)
#define SHUFFLER_MIN_CANT                1
#define SHUFFLER_MAX_CANT                3

//=====[API de bajo nivel: un motor por llamada, por eso las funciones se llaman motorShuffler]=====

// Configura pines y PWM. Deja ambos motores apagados y deshabilitados.
error_t motorShufflerInit(void* param);

// Habilita / deshabilita el motor (deshabilitar también lo detiene).
error_t motorShufflerEnable(shuffle_motor_t motor);
error_t motorShufflerDisable(shuffle_motor_t motor);

// Guarda la velocidad (0-100 %). Si el motor ya está andando, se aplica al instante.
error_t motorShufflerSetSpeed(uint8_t speed, shuffle_motor_t motor);

// Prende el motor con la velocidad configurada / lo apaga.
error_t motorShufflerStart(shuffle_motor_t motor);
error_t motorShufflerStop(shuffle_motor_t motor);

//=====[API de alto nivel: tarea de FreeRTOS, por eso solo Shuffler (sin motor)]=====

// Inicializa los motores y crea la tarea del shuffler.
error_t shufflerInit(void);

// Tarea: prende alternadamente un motor y el otro, con duración aleatoria. Ahora static, porque es privada al .c
// void taskShuffler(void* param);

#endif
