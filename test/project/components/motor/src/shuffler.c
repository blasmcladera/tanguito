/*
 * shuffler.c
 *
 * Driver de 2 motores DC con L298N usando PWM en los enables (ENA / ENB) para variar 
 * la potencia de los motores y dirección fija. Incluye la tarea de FreeRTOS que los alterna.
 *
 * Idea general:
 *   - La velocidad de cada motor se controla con el duty del PWM que va al
 *     pin ENABLE de su puente H.
 *   - La dirección no se toca nunca: IN1 e IN4 quedan en alto por software y
 *     IN2 e IN3 están conectados a GND por hardware.
 *
 * El código está dividido en tres capas:
 *   1. Funciones auxiliares (static, privadas de este archivo).
 *   2. API de bajo nivel: Init / Enable / Disable / SetSpeed / Start / Stop.       (las funciones se llaman motorShuffler...)
 *   3. Tarea de FreeRTOS (taskShuffler) y su función de arranque (shufflerInit).   (las funciones se llaman solo shuffler)
 */

#include <stdlib.h>     // rand(), srand()
#include <stddef.h>     // NULL

#include "sapi.h"       // gpioInit, gpioWrite, pwmInit, pwmWrite
#include "FreeRTOS.h"   // tipos y configuración de FreeRTOS
#include "task.h"       // xTaskCreate, vTaskDelay, xTaskGetTickCount

#include "shuffler.h"   // declaraciones públicas de este módulo

//==================[constantes internas]========================================

/* Tamaño del stack de la tarea (en palabras, no en bytes). Se usa el doble del mínimo porque rand() 
 * y las llamadas a sAPI pueden necesitar más que el stack mínimo. (SI ALGO NO FUNCIONA PUEDE SER ESTO) */
#define SHUFFLER_TASK_STACK    (2 * configMINIMAL_STACK_SIZE)

// Prioridad de la tarea: una por encima de la tarea idle. 
#define SHUFFLER_TASK_PRIORITY (tskIDLE_PRIORITY + 1)

// pwmWrite() de sAPI recibe un duty de 0 a 255 (8 bits). 
#define PWM_DUTY_MAX           255

//==================[estado interno]=========================================

// Toda la información que el driver necesita recordar de cada motor. 
typedef struct {
   pwmMap_t pin;       // Pin PWM conectado al enable del motor
   uint8_t  speed;     // Velocidad configurada, en % (0-100)
   bool_t   enabled;   // TRUE si el motor tiene permiso para andar
   bool_t   running;   // TRUE si el motor está andando en este momento
} motorState_t;

/* Un registro por motor. Se indexa con el enum shuffle_motor_t:
 * motors[SHUFFLE_LEFT] y motors[SHUFFLE_RIGHT]. */
static motorState_t motors[SHUFFLE_MOTOR_COUNT];

/* TRUE una vez que motorShufflerInit() terminó bien.
 * Evita usar el driver antes de haberlo inicializado. */
static bool_t attached = FALSE;

//==================[funciones auxiliares]===================================

/* Devuelve TRUE si el driver está inicializado y el motor recibido es uno de los
 * motores válidos del enum (o sea no estamos intentando prender un motor número4). */
static bool_t motorIsValid( shuffle_motor_t motor )
{
   return ( attached && ( (int)motor >= 0 ) && ( motor < SHUFFLE_MOTOR_COUNT ) );
}

/* Convierte una velocidad en porcentaje (0-100) al duty que espera
 * pwmWrite() (0-255). Se hace la cuenta en 32 bits para no desbordar:
 * 100 * 255 = 25500, que no entra en un uint8_t.
 * Ejemplos: 0 -> 0, 50 -> 127, 100 -> 255. */
static uint8_t speedToDuty( uint8_t speed )
{
   return (uint8_t)( ( (uint32_t)speed * PWM_DUTY_MAX ) / MOTOR_SHUFFLER_MAX_SPEED );
}

//==================[API de bajo nivel]======================================

/* Configura el hardware y el estado interno. Debe llamarse una sola vez,
 * antes que cualquier otra función de este módulo.
 * El parámetro no se usa; está para respetar la firma definida en el header. */
error_t motorShufflerInit( void* param )
{
   (void)param;   // evita el warning de "parámetro sin usar"

   // Dirección fija: IN1 e IN4 como salidas y en alto.    
   gpioInit( MOTOR_SHUFFLER_IN1_PIN, GPIO_OUTPUT );
   gpioInit( MOTOR_SHUFFLER_IN4_PIN, GPIO_OUTPUT );
   gpioWrite( MOTOR_SHUFFLER_IN1_PIN, ON );
   gpioWrite( MOTOR_SHUFFLER_IN4_PIN, ON );

   // Primero se habilitan los timers (SCT) en modo PWM, y después cada salida PWM hacia los enables del L298N.
   pwmInit( 0, PWM_ENABLE );
   pwmInit( MOTOR_SHUFFLER_PWM_LEFT_PIN,  PWM_ENABLE_OUTPUT );
   pwmInit( MOTOR_SHUFFLER_PWM_RIGHT_PIN, PWM_ENABLE_OUTPUT );

   // Se asocia cada motor lógico (enum) con su pin físico. 
   motors[SHUFFLE_LEFT].pin  = MOTOR_SHUFFLER_PWM_LEFT_PIN;
   motors[SHUFFLE_RIGHT].pin = MOTOR_SHUFFLER_PWM_RIGHT_PIN;

   // Estado inicial de ambos motores: velocidad máxima configurada, pero deshabilitados y apagados.
   for( int i = 0; i < SHUFFLE_MOTOR_COUNT; i++ ) {
      motors[i].speed   = MOTOR_SHUFFLER_MAX_SPEED;
      motors[i].enabled = FALSE;
      motors[i].running = FALSE;
      pwmWrite( motors[i].pin, 0 );   // PWM en 0 = motor apagado
   }

   attached = TRUE;   // a partir de acá el resto de las funciones funcionan
   return TANGUITO_OK;
}

// Le da permiso al motor para andar. No lo prende: para eso está Start. 
error_t motorShufflerEnable( shuffle_motor_t motor )
{
   if( !motorIsValid( motor ) ) {
      return TANGUITO_INVALID_PARAM;   // motor inexistente o driver sin inicializar
   }

   motors[motor].enabled = TRUE;
   return TANGUITO_OK;
}

// Detiene el motor y le quita el permiso para andar. 
error_t motorShufflerDisable( shuffle_motor_t motor )
{
   if( !motorIsValid( motor ) ) {
      return TANGUITO_INVALID_PARAM;
   }

   motorShufflerStop( motor );     // primero se apaga el PWM
   motors[motor].enabled = FALSE;  // y después se quita el permiso
   return TANGUITO_OK;
}

/* Guarda la velocidad del motor (0-100 %).
 * Si el motor ya está andando, el cambio se aplica en el momento;
 * si está parado, se usará la próxima vez que se lo prenda con Start. */
error_t motorShufflerSetSpeed( uint8_t speed, shuffle_motor_t motor )
{
   // speed no puede pasar de 100 %. Si pasan un valor mayor a eso, en vez de cortar a 
   // 100 sin preguntar, se le avisa al llamador que el argumento era inválido.
   if( !motorIsValid( motor ) || ( speed > MOTOR_SHUFFLER_MAX_SPEED ) ) {
      return TANGUITO_INVALID_PARAM;
   }

   motors[motor].speed = speed;

   if( motors[motor].running ) {
      pwmWrite( motors[motor].pin, speedToDuty( speed ) );
   }

   return TANGUITO_OK;
}

// Prende el motor con la velocidad guardada. Requiere que esté habilitado.
error_t motorShufflerStart( shuffle_motor_t motor )
{
   if( !motorIsValid( motor ) ) {
      return TANGUITO_INVALID_PARAM;
   }

   if( !motors[motor].enabled ) {
      return TANGUITO_INVALID_STATE;   // el argumento es válido, pero falta Enable
   }

   pwmWrite( motors[motor].pin, speedToDuty( motors[motor].speed ) );
   motors[motor].running = TRUE;
   return TANGUITO_OK;
}

/* Apaga el motor (PWM en 0). Se puede llamar aunque ya esté apagado
 * o no esté habilitado, no tiene efectos secundarios. */
error_t motorShufflerStop( shuffle_motor_t motor )
{
   if( !motorIsValid( motor ) ) {
      return TANGUITO_INVALID_PARAM;
   }

   pwmWrite( motors[motor].pin, 0 );
   motors[motor].running = FALSE;
   return TANGUITO_OK;
}

//==================[tarea de FreeRTOS]======================================

/* Tarea principal del shuffler.
 * Prende un motor, espera un tiempo aleatorio de 1 a 3 s, y pasa al otro.
 * Los motores siempre se prenden al 100 % (se configuró en shufflerInit). */
static void taskShuffler( void* param )
{
   shuffle_motor_t active = SHUFFLE_LEFT;   // motor que arranca prendido

   (void)param;   // la tarea no recibe parámetros

   /* Semilla del generador de números aleatorios. Por ahora no es aleatorio porque el tick
    * cuando arranca la función es siempre el mismo, la secuencia entonces sería la misma.
    * Eventualmente se debería hacer random en serio con una entrada del ADC que tenga ruido */
   srand( xTaskGetTickCount() );

   while( TRUE ) {
      // El motor "idle" es el que descansa en este turno. 
      shuffle_motor_t idle = ( active == SHUFFLE_LEFT ) ? SHUFFLE_RIGHT : SHUFFLE_LEFT;

      /* Duración del turno: valor entre SHUFFLER_MIN_CANT y
       * SHUFFLER_MAX_CANT, ambos incluidos.
       * rand() % 2001 da un número de 0 a 2000, y se le suma 1000. */
      uint32_t durationMs = (SHUFFLER_MIN_CANT +
                            ( rand() % ( SHUFFLER_MAX_CANT - SHUFFLER_MIN_CANT + 1 ) ))*1000;

      /* Primero se apaga el que descansa y después se prende el activo,
       * así nunca hay dos motores prendidos a la vez.
       * Los códigos de retorno se ignoran: la tarea no tiene a quién
       * reportar un error, y si shufflerInit() salió bien no deberían fallar. */
      motorShufflerStop( idle );
      motorShufflerStart( active );

      /* Se bloquea la tarea (no la CPU) mientras dura el turno.
       * Otras tareas, como el parpadeo del LED, siguen corriendo. */
      vTaskDelay( pdMS_TO_TICKS( durationMs ) );

      active = idle;   // el próximo turno le toca al otro motor
   }
}

/* Función de arranque del módulo: inicializa los motores y crea la tarea.
 * Es lo único que tiene que llamar el main. Debe llamarse antes de
 * vTaskStartScheduler(). */
error_t shufflerInit( void )
{
   error_t err;

   // 1) Hardware: pines, PWM y estado interno. 
   err = motorShufflerInit( NULL );
   if( !tanguitoErrorIsOk( err ) ) {
      return err;   // se propaga el error hacia quien llamó
   }

   /* 2) Ambos motores habilitados y configurados al 100 % de potencia.
    * Se chequea cada retorno y se propaga el primer error que aparezca. */
   for( int i = 0; i < SHUFFLE_MOTOR_COUNT; i++ ) {
      err = motorShufflerEnable( (shuffle_motor_t)i );
      if( !tanguitoErrorIsOk( err ) ) {
         return err;   // se propaga el error hacia quien llamó
      }      

      err = motorShufflerSetSpeed( MOTOR_SHUFFLER_MAX_SPEED, (shuffle_motor_t)i );
      if( !tanguitoErrorIsOk( err ) ) {
         return err;   // se propaga el error hacia quien llamó
      }
   }

   /* 3) Se crea la tarea. xTaskCreate devuelve pdPASS (cte de freeRTOS de éxito) si pudo 
    * reservar memoria del heap de FreeRTOS para el stack y el TCB de la tarea. */
   if( xTaskCreate( taskShuffler,           // función de la tarea
                    "taskShuffler",         // nombre (para debug)
                    SHUFFLER_TASK_STACK,    // stack
                    NULL,                   // parámetro de la tarea
                    SHUFFLER_TASK_PRIORITY, // prioridad
                    NULL ) != pdPASS ) {    // no necesitamos el handle
      return TANGUITO_NO_MEM;   // no alcanzó el heap
   }

   return TANGUITO_OK;
}