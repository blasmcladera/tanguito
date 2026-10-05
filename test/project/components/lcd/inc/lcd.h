/*
 * i2c_lcd.h
 *
 * Driver de LCD de caracteres HD44780 (16x2, 20x4, ...) conectado por I2C
 * mediante un "backpack" con PCF8574, para EDU-CIAA-NXP + sAPI + FreeRTOS.
 *
 * DISENO
 *  - Una unica tarea ("LcdTask") es la duena del bus I2C0 y del LCD.
 *  - El resto de las tareas solo ENCOLAN pedidos (i2cLcdPrint, i2cLcdClear...)
 *    y vuelven enseguida: no se bloquean esperando al LCD, y no hay problemas
 *    de reentrancia (a diferencia del driver LCD original de sAPI).
 *  - Los delays del LCD usan vTaskDelay (no hay busy-wait largos).
 *  - Si el LCD no responde (mal cableado, sin alimentacion) la tarea reintenta
 *    la inicializacion cada I2C_LCD_RETRY_MS y descarta los pedidos mientras
 *    tanto. i2cLcdIsReady() permite saber en que estado esta.
 *
 * USO MINIMO
 *    boardConfig();
 *    i2cLcdInit();                       // antes de vTaskStartScheduler()
 *    ...
 *    i2cLcdPrintLine(0, "Hola mundo");   // desde cualquier tarea
 *    i2cLcdPrint(1, 0, "Jugadores:");
 *    i2cLcdPrintInt(1, 11, 4, 2);
 *
 * NOTAS
 *  - Solo texto ASCII. Las tildes y la enie NO existen en la ROM estandar del
 *    LCD (salen caracteres raros): usar "Anio" en lugar de la palabra con enie,
 *    o definir un caracter propio con i2cLcdCreateChar().
 *  - Las funciones NO se pueden llamar desde una ISR.
 *  - Las funciones devuelven TRUE si el pedido ENTRO en la cola (no si ya se
 *    dibujo en pantalla, eso pasa unos ms despues).
 *  - Si mas adelante se agrega otro dispositivo en I2C0, hay que compartir el
 *    bus con un mutex: hoy solo este modulo lo usa.
 */

#ifndef _I2C_LCD_H_
#define _I2C_LCD_H_

#include <stdint.h>
#include "sapi_datatypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==================[configuracion de hardware]==============================*/

/* Tamano del display. Todas las macros se pueden pisar con -D en el Makefile. */
#ifndef I2C_LCD_COLS
#define I2C_LCD_COLS              16
#endif

#ifndef I2C_LCD_ROWS
#define I2C_LCD_ROWS              2
#endif

#if ( I2C_LCD_COLS < 1 ) || ( I2C_LCD_COLS > 20 )
#error "I2C_LCD_COLS debe estar entre 1 y 20"
#endif
#if ( I2C_LCD_ROWS < 1 ) || ( I2C_LCD_ROWS > 4 )
#error "I2C_LCD_ROWS debe estar entre 1 y 4"
#endif

/*
 * Direccion I2C de 7 bits del PCF8574 (sin el bit R/W).
 * Los backpack comunes salen con 0x27 (PCF8574T) o 0x3F (PCF8574AT).
 * Se prueba primero PRIMARY y despues SECONDARY (0 = no probar).
 */
#ifndef I2C_LCD_ADDR_PRIMARY
#define I2C_LCD_ADDR_PRIMARY      0x27
#endif

#ifndef I2C_LCD_ADDR_SECONDARY
#define I2C_LCD_ADDR_SECONDARY    0x3F
#endif

#ifndef I2C_LCD_I2C_CLOCK_HZ
#define I2C_LCD_I2C_CLOCK_HZ      100000
#endif

/*
 * Conexion PCF8574 -> LCD. Este es el mapeo del backpack mas comun:
 *   P0=RS  P1=RW  P2=E  P3=Backlight  P4..P7=D4..D7
 * Si tu placa es distinta, pisar estas macros.
 */
#ifndef I2C_LCD_PIN_RS
#define I2C_LCD_PIN_RS            ( 1 << 0 )
#endif
#ifndef I2C_LCD_PIN_RW
#define I2C_LCD_PIN_RW            ( 1 << 1 )   /* siempre en 0 (solo escritura) */
#endif
#ifndef I2C_LCD_PIN_EN
#define I2C_LCD_PIN_EN            ( 1 << 2 )
#endif
#ifndef I2C_LCD_PIN_BL
#define I2C_LCD_PIN_BL            ( 1 << 3 )
#endif
#ifndef I2C_LCD_DATA_SHIFT
#define I2C_LCD_DATA_SHIFT        4            /* D4 en P4 */
#endif

/*==================[configuracion de FreeRTOS]==============================*/

#ifndef I2C_LCD_TASK_STACK_WORDS
#define I2C_LCD_TASK_STACK_WORDS  256          /* palabras de 32 bits = 1 KB */
#endif

#ifndef I2C_LCD_TASK_PRIORITY
#define I2C_LCD_TASK_PRIORITY     1            /* igual que tskIDLE_PRIORITY + 1 */
#endif

#ifndef I2C_LCD_QUEUE_LEN
#define I2C_LCD_QUEUE_LEN         8            /* pedidos pendientes */
#endif

/* Cuanto espera el que llama si la cola esta llena. */
#ifndef I2C_LCD_ENQUEUE_TIMEOUT_MS
#define I2C_LCD_ENQUEUE_TIMEOUT_MS 20
#endif

/* Cada cuanto reintenta inicializar si el LCD no responde. */
#ifndef I2C_LCD_RETRY_MS
#define I2C_LCD_RETRY_MS          1000
#endif

/*
 * Limite de iteraciones de polling esperando un cambio de estado del bus.
 * Evita que un bus trabado (SDA/SCL a masa, sin pull-ups) cuelgue la tarea
 * para siempre, que es lo que hace Chip_I2CM_XferBlocking de LPCOpen.
 */
#ifndef I2C_LCD_SPIN_LIMIT
#define I2C_LCD_SPIN_LIMIT        1000000UL
#endif

/*==================[tipos]==================================================*/

typedef enum {
   I2C_LCD_CURSOR_OFF      = 0,   /* sin cursor                 */
   I2C_LCD_CURSOR_BLINK    = 1,   /* bloque parpadeante         */
   I2C_LCD_CURSOR_UNDER    = 2,   /* guion bajo fijo            */
   I2C_LCD_CURSOR_UNDER_BLINK = 3 /* guion bajo + bloque        */
} i2cLcdCursor_t;

/*==================[funciones publicas]=====================================*/

/*
 * Inicializa I2C0, crea la cola y la tarea del LCD.
 * Llamar una sola vez desde main(), despues de boardConfig() y antes de
 * vTaskStartScheduler(). Devuelve FALSE si no hubo memoria en el heap.
 * La inicializacion del LCD en si la hace la tarea cuando arranca el scheduler.
 */
bool_t i2cLcdInit( void );

/* TRUE si el LCD respondio y esta inicializado. */
bool_t i2cLcdIsReady( void );

/* Direccion I2C detectada (7 bits), o 0 si todavia no se encontro el LCD. */
uint8_t i2cLcdGetAddress( void );

/* Borra la pantalla. */
bool_t i2cLcdClear( void );

/* Escribe 'text' desde (row, col). Se recorta al borde derecho de la fila. */
bool_t i2cLcdPrint( uint8_t row, uint8_t col, const char* text );

/* Escribe 'text' en toda la fila: lo que sobra se rellena con espacios. */
bool_t i2cLcdPrintLine( uint8_t row, const char* text );

/*
 * Escribe un entero con signo en (row, col), alineado a la derecha en 'width'
 * columnas. No usa printf: consume muy poco stack, ideal para tareas con
 * configMINIMAL_STACK_SIZE.
 */
bool_t i2cLcdPrintInt( uint8_t row, uint8_t col, int32_t value, uint8_t width );

/*
 * Version con formato. CUIDADO: usa vsnprintf en el stack de la tarea que
 * llama (necesita ~512 bytes libres). Con configMINIMAL_STACK_SIZE (400 bytes)
 * usar i2cLcdPrintInt / i2cLcdPrint en su lugar. No soporta %f con nano.specs.
 */
bool_t i2cLcdPrintf( uint8_t row, uint8_t col, const char* fmt, ... );

/* Escribe el caracter 'code' (0..255, incluye los 0..7 de createChar). */
bool_t i2cLcdWriteChar( uint8_t row, uint8_t col, uint8_t code );

/* Enciende / apaga la luz de fondo. */
bool_t i2cLcdBacklight( bool_t on );

/* Cursor visible / parpadeante. */
bool_t i2cLcdCursor( i2cLcdCursor_t mode );

/*
 * Define un caracter propio (slot 0..7) con un bitmap de 8 filas de 5 bits.
 * Despues se muestra con i2cLcdWriteChar(row, col, slot).
 */
bool_t i2cLcdCreateChar( uint8_t slot, const uint8_t bitmap[8] );

#ifdef __cplusplus
}
#endif

#endif /* _I2C_LCD_H_ */