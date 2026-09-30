/*
 * stepper.h
 *
 * Driver sencillo para un motor paso a paso bipolar controlado mediante
 * un A4988 en EDU-CIAA-NXP / sAPI.
 *
 * DiseÃ±o deliberadamente de un solo motor: el A4988 hace la secuenciaciÃ³n
 * de las fases internamente. La CIAA solamente genera pulsos STEP y fija DIR.
 */

#ifndef _STEPPER_H_
#define _STEPPER_H_

#include "sapi_datatypes.h"
#include "sapi_peripheral_map.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==================[configuracion de hardware]==============================*/

/*
 * CAMBIAR ESTAS TRES DEFINICIONES SEGUN EL CABLEADO REAL.
 *
 * STEP  -> entrada STEP del A4988
 * DIR   -> entrada DIR del A4988
 * ENABLE-> entrada ENABLE (activo en LOW)
 *
 * RESET y SLEEP deben mantenerse en HIGH para operar normalmente.
 * MS1/MS2/MS3 pueden fijarse por hardware segun el microstepping deseado.
 */
#ifndef STEPPER_STEP_PIN
#define STEPPER_STEP_PIN      GPIO2
#endif

#ifndef STEPPER_DIR_PIN
#define STEPPER_DIR_PIN       GPIO3
#endif

#ifndef STEPPER_ENABLE_PIN
#define STEPPER_ENABLE_PIN    T_FIL2
#endif

#ifndef STEPPER_TIMER
#define STEPPER_TIMER TIMER1
#endif

/* Duracion del pulso HIGH de STEP.
 * El A4988 especifica minimo 1 us HIGH y 1 us LOW.
 * Se usan 2 us para dejar margen.
 */
#define STEPPER_STEP_HIGH_US 2UL
#define STEPPER_STEP_LOW_US  2UL


#define STEPPER_STEP_ANGLE_DEG   1.8f   // grados por paso completo del NEMA 

/* Relación = dientes del engranaje grande / dientes del engranaje del motor.
 * Ej: motor con 20 dientes y engranaje de 100 -> 5.0f
 * Por ahora 1.0f (sin reducción) hasta saber los valores reales. */
#define STEPPER_GEAR_RATIO       1.0f

#define STEPPER_MICROSTEPS       1      /* 1, 2, 4, 8 o 16 según MS1/MS2/MS3 del A4988 */


/*==================[tipos]==================================================*/

typedef enum {
   STEPPER_DIRECTION_FORWARD = 0,
   STEPPER_DIRECTION_REVERSE = 1
} stepperDirection_t;

/*==================[funciones publicas]====================================*/

/* Inicializa la interfaz GPIO del A4988. */
bool_t stepperInit( void );

/* Habilita/deshabilita fisicamente el driver A4988. */
bool_t stepperEnable( void );
bool_t stepperDisable( void );

/* Configura el sentido logico de giro. */
bool_t stepperSetDirection( stepperDirection_t direction );

/*
 * Configura la velocidad en pulsos STEP por segundo.
 * Ejemplo: 1000 -> 1000 microsteps/steps por segundo.
 * Debe configurarse antes de iniciar un movimiento.
 */
bool_t stepperSetSpeed( uint32_t stepsPerSecond );

/*
 * Inicia un movimiento no bloqueante de 'steps' pulsos.
 *
 * La funcion devuelve TRUE si el movimiento fue aceptado.
 * Mientras el movimiento esta en curso, stepperIsBusy() devuelve TRUE.
 */
bool_t stepperMove( uint32_t steps );

/* Detiene el movimiento actual y deja STEP en LOW. */
void stepperStop( void );

/* TRUE mientras quedan pulsos por generar. */
bool_t stepperIsBusy( void );


/* Convierte la cantidad de grados en Steps que hace el suffler suponiendo que cada steps es 1,8 Grados*/
bool_t turn_Degrees( float Degrees);



/*==================[callbacks internos expuestos a sAPI Timer]============*/

void stepperTimerCompareMatch0func( void* ptr );
void stepperTimerCompareMatch1func( void* ptr );

#ifdef __cplusplus
}
#endif

#endif /* _STEPPER_H_ */
