/*
 * tanguito_error.c
 *
 * Funciones auxiliares para trabajar con error_t.
 * Como los structs no se pueden comparar con == en C, el chequeo
 * de errores se hace siempre a través de estas funciones.
 */

#include <stddef.h>   // NULL
#include "tanguito_error.h"

/* Un error es "OK" si y sólo si su código es el de TANGUITO_OK (0).
 * Se compara contra TANGUITO_OK.code y no contra un 0 suelto para que,
 * si algún día cambia el código de OK, esto siga funcionando. REUTILIZABLE AS FUCK*/
bool tanguitoErrorIsOk( error_t err )
{
   return ( err.code == TANGUITO_OK.code );
}

/* Dos errores son iguales si tienen el mismo código.
 * El mensaje no se compara: es solo informativo, y dos punteros a
 * strings iguales no tienen por qué apuntar a la misma dirección. */
bool tanguitoErrorEquals( error_t a, error_t b )
{
   return ( a.code == b.code );
}

/* Devuelve el mensaje del error. Si alguien armó un error_t a mano
 * y dejó el mensaje en NULL, devuelve un texto por defecto en lugar
 * de NULL, así se puede imprimir sin riesgo de que el programa falle. */
const char* tanguitoErrorMessage( error_t err )
{
   return ( err.message != NULL ) ? err.message : "Sin mensaje";
}