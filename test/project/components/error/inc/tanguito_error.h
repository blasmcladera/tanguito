#ifndef _TANGUITO_ERROR_H_
#define _TANGUITO_ERROR_H_

#include <stdio.h>
#include <stdint.h>    // uint8_t
#include <stdbool.h>   // bool

/*
 * Convención de errores del proyecto.
 *
 * Un error_t es un struct con un código numérico (para comparar desde el
 * código) y un mensaje legible (para mostrar o loguear).
 *
 * Lo iremos complejizando a medida que sea necesario.
 *
 * IMPORTANTE: como en C los structs NO se pueden comparar con == o !=,
 * para chequear un error usamos las funciones de abajo:
 *    error_t err = shufflerInit();
 *    if( !tanguitoErrorIsOk( err ) ) { ... }
 *    if( tanguitoErrorEquals( err, TANGUITO_NO_MEM ) ) { ... }
 */
 
typedef struct {
   uint8_t     code;      // Código del error (0 = OK)
   const char *message;   // Texto descriptivo (siempre un literal, no se libera)
} error_t;

//==================[errores definidos]======================================

#define TANGUITO_OK              ((error_t){0, "OK"})
#define TANGUITO_ERROR           ((error_t){1, "Error"})
#define TANGUITO_INVALID_PARAM   ((error_t){2, "Parámetro inválido"})
#define TANGUITO_INVALID_STATE   ((error_t){3, "Estado inválido"})
#define TANGUITO_NO_MEM          ((error_t){4, "Sin memoria"})

//==================[funciones]======================================

// true si el error es TANGUITO_OK (código 0)
bool tanguitoErrorIsOk( error_t err );

// true si ambos errores tienen el mismo código (el mensaje no se compara, sólo número)
bool tanguitoErrorEquals( error_t a, error_t b );

// Devuelve el mensaje del error; nunca devuelve NULL para evitar errores
// Si no tiene mensaje predefinido escribe "Sin comentario"
const char* tanguitoErrorMessage( error_t err );

#endif