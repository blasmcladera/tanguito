#ifndef ROTATOR_H_
#define ROTATOR_H_

#include "sapi.h"

/*
 * ROTATOR: capa de abstraccion sobre el stepper.
 *
 * - El stepper (stepper.c) solo sabe de pasos y direccion del motor.
 * - El rotator trabaja en GRADOS del engranaje de salida (la base circular
 *   que posiciona las cartas) y decide a que angulo va cada carta segun la
 *   configuracion (jugadores, cartas, arco de 180 o 360).
 *
 * Referencia de angulos: 0 grados = posicion del primer jugador.
 */

/* Vueltas del motor por cada vuelta del engranaje de salida
 * = dientes de la base circular / dientes del pinon del motor.
 * PLACEHOLDER: poner los dientes reales. */
#define ROTATOR_GEAR_RATIO   ( 60.0f / 20.0f )

/* Task: espera DEAL_START, reparte todas las cartas y avisa DEAL_DONE */
void RotatorTask( void * params );

/* Angulo actual del repartidor (grados, referido al engranaje de salida) */
float rotatorGetAngle( void );

/* Para fijar la referencia (ej: despues de un homing o al arrancar) */
void rotatorSetAngle( float Degrees );

#endif /* ROTATOR_H_ */