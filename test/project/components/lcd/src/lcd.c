/*
 * i2c_lcd.c
 *
 * Driver de LCD HD44780 por I2C (PCF8574) para EDU-CIAA-NXP + sAPI + FreeRTOS.
 * Ver i2c_lcd.h para la descripcion general y el uso.
 */

/*==================[inclusions]=============================================*/

#include "lcd.h"

#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <stdarg.h>
#include <stdio.h>

/*==================[macros and definitions]=================================*/

/* Comandos del HD44780 */
#define HD_CMD_CLEAR            0x01
#define HD_CMD_ENTRY_MODE       0x06   /* cursor incrementa, sin shift      */
#define HD_CMD_DISPLAY_CTRL     0x08   /* | 0x04 display on | 0x02 | 0x01   */
#define HD_CMD_FUNCTION_SET     ( 0x20 | ( ( I2C_LCD_ROWS > 1 ) ? 0x08 : 0x00 ) ) /* 4 bits, 5x8 */
#define HD_CMD_SET_CGRAM        0x40
#define HD_CMD_SET_DDRAM        0x80

/* Maximo de bytes de datos que viaja en un pedido (texto o bitmap de 8). */
#define ILCD_PAYLOAD_MAX         ( ( I2C_LCD_COLS > 8 ) ? I2C_LCD_COLS : 8 )

/*
 * Delay en ms garantizando "al menos" ese tiempo. vTaskDelay(n) puede dormir
 * hasta un tick menos de lo pedido (el primer tick es parcial), por eso +1.
 */
#define ILCD_TICKS_MIN( ms )     ( ( ( pdMS_TO_TICKS( ms ) > 0 ) ? pdMS_TO_TICKS( ms ) : 1 ) + 1 )

typedef enum {
   ILCD_MSG_CLEAR = 0,
   ILCD_MSG_PRINT,
   ILCD_MSG_PRINT_LINE,
   ILCD_MSG_WRITE_CHAR,
   ILCD_MSG_BACKLIGHT,
   ILCD_MSG_CURSOR,
   ILCD_MSG_CREATE_CHAR
} ilcdMsgType_t;

typedef struct {
   uint8_t type;
   uint8_t row;
   uint8_t col;        /* en BACKLIGHT: 0/1; en CURSOR: modo; en CREATE_CHAR: slot */
   uint8_t len;
   uint8_t data[ ILCD_PAYLOAD_MAX ];
} ilcdMsg_t;

/*==================[internal data definition]===============================*/

static QueueHandle_t ilcdQueue = NULL;

/* Los escribe solo ILcdTask; los lee cualquiera (por eso volatile). */
static volatile bool_t  ilcdReady   = FALSE;
static volatile uint8_t ilcdAddress = 0;

/* Estado de la luz de fondo: bit BL del PCF8574. Solo lo toca ILcdTask. */
static uint8_t ilcdBacklightMask = I2C_LCD_PIN_BL;

/*==================[internal functions definition: bus I2C]=================*/

static void ilcdDelayMs( uint32_t ms )
{
   vTaskDelay( ILCD_TICKS_MIN( ms ) );
}

/*
 * Escritura I2C que SI informa si el esclavo contesto.
 *
 * No se usa i2cWrite() de sAPI porque devuelve TRUE aunque el esclavo haga
 * NAK (Chip_I2CM_XferBlocking retorna "termino", no "termino bien"), y porque
 * su espera no tiene timeout. Aca se mira el status real y se acota el polling.
 */
static bool_t ilcdBusWrite( uint8_t address, const uint8_t* buffer, uint16_t size )
{
   I2CM_XFER_T xfer;
   uint32_t done = 0;

   xfer.slaveAddr = address;       /* 7 bits: LPCOpen lo desplaza solo */
   xfer.options   = 0;
   xfer.status    = 0;
   xfer.txBuff    = buffer;
   xfer.txSz      = size;
   xfer.rxBuff    = 0;
   xfer.rxSz      = 0;

   Chip_I2CM_Xfer( LPC_I2C0, &xfer );

   while( done == 0 ) {
      uint32_t guard = I2C_LCD_SPIN_LIMIT;

      while( Chip_I2CM_StateChanged( LPC_I2C0 ) == 0 ) {
         if( --guard == 0 ) {
            Chip_I2CM_ResetControl( LPC_I2C0 );   /* bus trabado: abortar */
            return FALSE;
         }
      }
      done = Chip_I2CM_XferHandler( LPC_I2C0, &xfer );
   }

   return ( xfer.status == I2CM_STATUS_OK ) ? TRUE : FALSE;
}

static bool_t ilcdExpanderWrite( const uint8_t* buffer, uint16_t size )
{
   return ilcdBusWrite( ilcdAddress, buffer, size );
}

/*==================[internal functions definition: HD44780]=================*/

/* Un nibble crudo (solo para la secuencia de arranque en modo 8 -> 4 bits). */
static bool_t ilcdWriteNibbleRaw( uint8_t nibble )
{
   uint8_t d = (uint8_t)( ( nibble & 0x0F ) << I2C_LCD_DATA_SHIFT ) | ilcdBacklightMask;
   uint8_t b[2];

   b[0] = d | I2C_LCD_PIN_EN;   /* E = 1 : el LCD captura en el flanco de bajada */
   b[1] = d;                    /* E = 0 */
   return ilcdExpanderWrite( b, 2 );
}

/*
 * Convierte 'count' bytes en la secuencia para el PCF8574 (4 bytes por byte
 * de LCD: nibble alto con E=1, E=0, nibble bajo con E=1, E=0) y la manda en UNA
 * sola transaccion I2C. Cada byte I2C dura ~90 us a 100 kHz, asi que el pulso
 * de E y el tiempo entre comandos (37 us) se cumplen de sobra sin delays.
 */
static bool_t ilcdWriteBytes( const uint8_t* values, uint8_t count, bool_t isData )
{
   uint8_t buf[ 4 * ILCD_PAYLOAD_MAX ];
   uint8_t ctl = ilcdBacklightMask | ( isData ? I2C_LCD_PIN_RS : 0 );
   uint8_t i;

   if( count == 0 ) {
      return TRUE;
   }
   if( count > ILCD_PAYLOAD_MAX ) {
      count = ILCD_PAYLOAD_MAX;
   }

   for( i = 0; i < count; i++ ) {
      uint8_t hi = (uint8_t)( ( ( values[i] >> 4 ) & 0x0F ) << I2C_LCD_DATA_SHIFT ) | ctl;
      uint8_t lo = (uint8_t)( ( values[i] & 0x0F ) << I2C_LCD_DATA_SHIFT ) | ctl;

      buf[4*i + 0] = hi | I2C_LCD_PIN_EN;
      buf[4*i + 1] = hi;
      buf[4*i + 2] = lo | I2C_LCD_PIN_EN;
      buf[4*i + 3] = lo;
   }
   return ilcdExpanderWrite( buf, (uint16_t)( 4 * count ) );
}

static bool_t ilcdCommand( uint8_t cmd )
{
   return ilcdWriteBytes( &cmd, 1, FALSE );
}

static bool_t ilcdSetCursorPos( uint8_t row, uint8_t col )
{
   static const uint8_t rowOffset[4] = { 0x00, 0x40, I2C_LCD_COLS, 0x40 + I2C_LCD_COLS };

   return ilcdCommand( HD_CMD_SET_DDRAM | (uint8_t)( rowOffset[row] + col ) );
}

/* Prueba si hay un PCF8574 respondiendo en 'address'. */
static bool_t ilcdProbe( uint8_t address )
{
   return ilcdBusWrite( address, &ilcdBacklightMask, 1 );
}

/* Busca el backpack y corre la secuencia de inicializacion del datasheet. */
static bool_t ilcdBringUp( void )
{
   static const uint8_t candidates[2] = { I2C_LCD_ADDR_PRIMARY, I2C_LCD_ADDR_SECONDARY };
   uint8_t i;
   bool_t ok;

   ilcdAddress = 0;
   for( i = 0; i < 2; i++ ) {
      if( candidates[i] != 0 && ilcdProbe( candidates[i] ) ) {
         ilcdAddress = candidates[i];
         break;
      }
   }
   if( ilcdAddress == 0 ) {
      return FALSE;
   }

   /* Secuencia de reset por software (HD44780, figura "4-bit interface"). */
   ilcdDelayMs( 50 );                       /* > 40 ms despues de VCC            */
   ok  = ilcdWriteNibbleRaw( 0x03 );  ilcdDelayMs( 5 );   /* > 4.1 ms */
   ok &= ilcdWriteNibbleRaw( 0x03 );  ilcdDelayMs( 1 );   /* > 100 us */
   ok &= ilcdWriteNibbleRaw( 0x03 );  ilcdDelayMs( 1 );
   ok &= ilcdWriteNibbleRaw( 0x02 );  ilcdDelayMs( 1 );   /* pasa a 4 bits */

   ok &= ilcdCommand( HD_CMD_FUNCTION_SET );
   ok &= ilcdCommand( HD_CMD_DISPLAY_CTRL );             /* display off */
   ok &= ilcdCommand( HD_CMD_CLEAR );  ilcdDelayMs( 3 );  /* 1.52 ms     */
   ok &= ilcdCommand( HD_CMD_ENTRY_MODE );
   ok &= ilcdCommand( HD_CMD_DISPLAY_CTRL | 0x04 );      /* display on, sin cursor */

   return ok ? TRUE : FALSE;
}

/*==================[internal functions definition: tarea]===================*/

/* Ejecuta un pedido. Devuelve FALSE si hubo error de bus. */
static bool_t ilcdHandleMsg( const ilcdMsg_t* m )
{
   bool_t ok = TRUE;
   uint8_t line[ I2C_LCD_COLS ];
   uint8_t i;

   switch( m->type ) {

   case ILCD_MSG_CLEAR:
      ok = ilcdCommand( HD_CMD_CLEAR );
      ilcdDelayMs( 3 );                               /* 1.52 ms */
      break;

   case ILCD_MSG_PRINT:
      ok  = ilcdSetCursorPos( m->row, m->col );
      ok &= ilcdWriteBytes( m->data, m->len, TRUE );
      break;

   case ILCD_MSG_PRINT_LINE:
      for( i = 0; i < I2C_LCD_COLS; i++ ) {
         line[i] = ( i < m->len ) ? m->data[i] : ' ';
      }
      ok  = ilcdSetCursorPos( m->row, 0 );
      ok &= ilcdWriteBytes( line, I2C_LCD_COLS, TRUE );
      break;

   case ILCD_MSG_WRITE_CHAR:
      ok  = ilcdSetCursorPos( m->row, m->col );
      ok &= ilcdWriteBytes( m->data, 1, TRUE );
      break;

   case ILCD_MSG_BACKLIGHT:
      ilcdBacklightMask = ( m->col != 0 ) ? I2C_LCD_PIN_BL : 0;
      ok = ilcdExpanderWrite( &ilcdBacklightMask, 1 );
      break;

   case ILCD_MSG_CURSOR:
      ok = ilcdCommand( HD_CMD_DISPLAY_CTRL | 0x04 | ( m->col & 0x03 ) );
      break;

   case ILCD_MSG_CREATE_CHAR:
      ok  = ilcdCommand( HD_CMD_SET_CGRAM | (uint8_t)( ( m->col & 0x07 ) << 3 ) );
      ok &= ilcdWriteBytes( m->data, 8, TRUE );
      ok &= ilcdCommand( HD_CMD_SET_DDRAM );          /* vuelve a DDRAM */
      break;

   default:
      break;
   }

   return ok ? TRUE : FALSE;
}

static void ILcdTask( void* pvParameters )
{
   ilcdMsg_t msg;

   ( void )pvParameters;

   for( ;; ) {

      if( !ilcdReady ) {
         if( ilcdBringUp() ) {
            ilcdReady = TRUE;
         } else {
            xQueueReset( ilcdQueue );                 /* no dejar la cola llena */
            vTaskDelay( ILCD_TICKS_MIN( I2C_LCD_RETRY_MS ) );
            continue;
         }
      }

      if( xQueueReceive( ilcdQueue, &msg, portMAX_DELAY ) == pdTRUE ) {
         if( !ilcdHandleMsg( &msg ) ) {
            ilcdReady = FALSE;                        /* error de bus: re-inicializar */
         }
      }
   }
}

/*==================[internal functions definition: cola]====================*/

static bool_t ilcdSend( const ilcdMsg_t* msg )
{
   if( ilcdQueue == NULL ) {
      return FALSE;
   }
   return ( xQueueSend( ilcdQueue, msg, ILCD_TICKS_MIN( I2C_LCD_ENQUEUE_TIMEOUT_MS ) ) == pdTRUE ) ? TRUE : FALSE;
}

/*==================[external functions definition]==========================*/

bool_t i2cLcdInit( void )
{
   if( ilcdQueue != NULL ) {
      return TRUE;                                   /* ya inicializado */
   }

   if( !i2cInit( I2C0, I2C_LCD_I2C_CLOCK_HZ ) ) {
      return FALSE;
   }

   ilcdQueue = xQueueCreate( I2C_LCD_QUEUE_LEN, sizeof( ilcdMsg_t ) );
   if( ilcdQueue == NULL ) {
      return FALSE;
   }

   if( xTaskCreate( ILcdTask, "LcdTask", I2C_LCD_TASK_STACK_WORDS, NULL,
                    I2C_LCD_TASK_PRIORITY, NULL ) != pdPASS ) {
      vQueueDelete( ilcdQueue );
      ilcdQueue = NULL;
      return FALSE;
   }

   return TRUE;
}

bool_t i2cLcdIsReady( void )
{
   return ilcdReady;
}

uint8_t i2cLcdGetAddress( void )
{
   return ilcdAddress;
}

bool_t i2cLcdClear( void )
{
   ilcdMsg_t m = { ILCD_MSG_CLEAR, 0, 0, 0, { 0 } };

   return ilcdSend( &m );
}

bool_t i2cLcdPrint( uint8_t row, uint8_t col, const char* text )
{
   ilcdMsg_t m = { ILCD_MSG_PRINT, 0, 0, 0, { 0 } };
   uint8_t room;

   if( text == NULL || row >= I2C_LCD_ROWS || col >= I2C_LCD_COLS ) {
      return FALSE;
   }

   room = (uint8_t)( I2C_LCD_COLS - col );
   while( text[m.len] != '\0' && m.len < room ) {
      m.data[m.len] = (uint8_t)text[m.len];
      m.len++;
   }
   if( m.len == 0 ) {
      return TRUE;                                   /* nada que escribir */
   }

   m.row = row;
   m.col = col;
   return ilcdSend( &m );
}

bool_t i2cLcdPrintLine( uint8_t row, const char* text )
{
   ilcdMsg_t m = { ILCD_MSG_PRINT_LINE, 0, 0, 0, { 0 } };

   if( text == NULL || row >= I2C_LCD_ROWS ) {
      return FALSE;
   }

   while( text[m.len] != '\0' && m.len < I2C_LCD_COLS ) {
      m.data[m.len] = (uint8_t)text[m.len];
      m.len++;
   }

   m.row = row;
   return ilcdSend( &m );
}

bool_t i2cLcdPrintInt( uint8_t row, uint8_t col, int32_t value, uint8_t width )
{
   char tmp[11];                                     /* hasta 10 digitos + signo */
   char out[ I2C_LCD_COLS + 1 ];
   uint32_t mag = ( value < 0 ) ? (uint32_t)( -( value + 1 ) ) + 1u : (uint32_t)value;
   uint8_t n = 0, pad, k = 0;

   do {
      tmp[n++] = (char)( '0' + ( mag % 10u ) );
      mag /= 10u;
   } while( mag != 0 && n < sizeof( tmp ) - 1 );
   if( value < 0 ) {
      tmp[n++] = '-';
   }

   pad = ( width > n ) ? (uint8_t)( width - n ) : 0;
   while( pad > 0 && k < I2C_LCD_COLS ) {
      out[k++] = ' ';
      pad--;
   }
   while( n > 0 && k < I2C_LCD_COLS ) {
      out[k++] = tmp[--n];
   }
   out[k] = '\0';

   return i2cLcdPrint( row, col, out );
}

bool_t i2cLcdPrintf( uint8_t row, uint8_t col, const char* fmt, ... )
{
   char buf[ I2C_LCD_COLS + 1 ];
   va_list ap;

   if( fmt == NULL ) {
      return FALSE;
   }

   va_start( ap, fmt );
   vsnprintf( buf, sizeof( buf ), fmt, ap );
   va_end( ap );

   return i2cLcdPrint( row, col, buf );
}

bool_t i2cLcdWriteChar( uint8_t row, uint8_t col, uint8_t code )
{
   ilcdMsg_t m = { ILCD_MSG_WRITE_CHAR, 0, 0, 1, { 0 } };

   if( row >= I2C_LCD_ROWS || col >= I2C_LCD_COLS ) {
      return FALSE;
   }

   m.row = row;
   m.col = col;
   m.data[0] = code;
   return ilcdSend( &m );
}

bool_t i2cLcdBacklight( bool_t on )
{
   ilcdMsg_t m = { ILCD_MSG_BACKLIGHT, 0, 0, 0, { 0 } };

   m.col = on ? 1 : 0;
   return ilcdSend( &m );
}

bool_t i2cLcdCursor( i2cLcdCursor_t mode )
{
   ilcdMsg_t m = { ILCD_MSG_CURSOR, 0, 0, 0, { 0 } };

   m.col = (uint8_t)mode;
   return ilcdSend( &m );
}

bool_t i2cLcdCreateChar( uint8_t slot, const uint8_t bitmap[8] )
{
   ilcdMsg_t m = { ILCD_MSG_CREATE_CHAR, 0, 0, 8, { 0 } };
   uint8_t i;

   if( bitmap == NULL || slot > 7 ) {
      return FALSE;
   }

   for( i = 0; i < 8; i++ ) {
      m.data[i] = bitmap[i] & 0x1F;
   }
   m.col = slot;
   return ilcdSend( &m );
}

/*==================[end of file]============================================*/