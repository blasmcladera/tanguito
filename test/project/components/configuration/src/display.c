/*
 * display.c
 *
 * Vista en el LCD de la configuracion del sistema. Ver display.h.
 *
 * ARQUITECTURA
 *   configuration.c --updateDisplay()--> [buzon de 1 lugar] --> DisplayTask --> i2c_lcd
 *
 *  - updateDisplay() solo copia un snapshot al buzon (xQueueOverwrite): no
 *    bloquea y la ultima llamada gana, asi que una rafla de botones nunca
 *    llena nada ni deja una pantalla vieja.
 *  - DisplayTask es la unica que decide que se dibuja. Ademas de reaccionar a
 *    los snapshots, anima: titileo del menu y spinner. Duerme en el propio
 *    xQueueReceive con timeout, sin tick propio ni timers de software.
 *  - Todos los estados de la vista (fase del titileo, cuadro del spinner,
 *    juego de caracteres cargado en la CGRAM) son privados de DisplayTask, por
 *    lo que no hay mutex.
 *  - Si el LCD se desconecta, no se manda nada; cuando vuelve, se redibuja la
 *    pantalla actual y se recargan los caracteres propios.
 */

/*==================[inclusions]=============================================*/

#include "display.h"
#include "lcd.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include <string.h>

/*==================[macros and definitions]=================================*/

#define DSP_COLS   I2C_LCD_COLS
#define DSP_ROWS   I2C_LCD_ROWS

#if ( DSP_COLS < 15 ) || ( DSP_ROWS < 2 )
#error "display.c necesita un LCD de al menos 15 columnas y 2 filas"
#endif
#if ( DISPLAY_SIGN_ROW < 0 ) || ( DISPLAY_SIGN_ROW > 1 )
#error "DISPLAY_SIGN_ROW debe ser 0 o 1"
#endif

#define DSP_SPIN_COL        ( ( DSP_COLS - 1 ) / 2 )
#define DSP_SPIN_FRAMES     4
#define DSP_DIGIT_CELLS     3          /* ancho de un digito grande          */
#define DSP_DIGIT_PITCH     4          /* digito + 1 columna de separacion   */
#define DSP_MAX_DIGITS      3          /* uint8_t: 0..255                    */
#define DSP_MAX_SLOT0       ( 2 * DSP_MAX_DIGITS )

/* Caracteres propios para los digitos grandes: indice = slot de la CGRAM. */
enum {
   G_LB = 0,   /* barra inferior            (slot 0: ver dspDrawEditNumber) */
   G_LT,       /* esquina superior izq.     */
   G_UB,       /* barra superior           */
   G_RT,       /* esquina superior der.     */
   G_LL,       /* esquina inferior izq.     */
   G_LR,       /* esquina inferior der.     */
   G_UMB,      /* barra superior + mitad (parte alta)  */
   G_LMB,      /* mitad (parte baja) + barra inferior  */
   G_COUNT
};
#define G_FULL    0xFF   /* bloque lleno de la ROM del LCD */
#define G_BLANK   ' '

typedef enum { VIEW_NONE = 0, VIEW_MENU, VIEW_EDIT, VIEW_LOADING } dspView_t;
typedef enum { GLYPHS_NONE = 0, GLYPHS_DIGITS, GLYPHS_SPINNER } dspGlyphs_t;

/* Lo que viaja de configuration.c a DisplayTask. */
typedef struct {
   uint8_t actual;      /* state_t */
   uint8_t selected;    /* state_t */
   uint8_t value;       /* numero en edicion (players_aux / cards_aux) */
} dspSnapshot_t;

/*==================[internal data definition]===============================*/

static QueueHandle_t dspMailbox = NULL;

/* Estado privado de DisplayTask. Todo en cero = "nada dibujado todavia". */
static struct {
   dspSnapshot_t snap;
   dspView_t     view;
   dspGlyphs_t   glyphs;
   bool_t        haveSnap;
   bool_t        dirty;        /* hay que redibujar toda la pantalla  */
   bool_t        wasReady;
   bool_t        blinkOn;
   bool_t        showTitle;
   uint8_t       spinFrame;
   TickType_t    nextBlink;
   TickType_t    nextSpin;
   TickType_t    titleEnd;
} dsp;

/* Opciones del menu, indexadas por state_t. */
static const char* const dspLabel[NUM_STATES] = {
   [SELECT_PLAYERS] = DISPLAY_TEXT_PLAYERS,
   [SELECT_CARDS]   = DISPLAY_TEXT_CARDS,
   [START]          = DISPLAY_TEXT_START
};

/*
 * Segmentos de los digitos grandes (5x8, bit 4 = columna izquierda).
 * Cada digito mide 3x2 celdas; el medio se arma con UMB (arriba) + LMB (abajo).
 */
static const uint8_t dspDigitGlyph[G_COUNT][8] = {
   /* G_LB  */ { 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F },
   /* G_LT  */ { 0x07, 0x0F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },
   /* G_UB  */ { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00 },
   /* G_RT  */ { 0x1C, 0x1E, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },
   /* G_LL  */ { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x0F, 0x07 },
   /* G_LR  */ { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1E, 0x1C },
   /* G_UMB */ { 0x1F, 0x1F, 0x1F, 0x00, 0x00, 0x00, 0x1F, 0x1F },
   /* G_LMB */ { 0x1F, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x1F, 0x1F }
};

/* digito -> [fila][columna] */
static const uint8_t dspBigDigit[10][2][DSP_DIGIT_CELLS] = {
   /* 0 */ { { G_LT,  G_UB,  G_RT   }, { G_LL,  G_LB,  G_LR  } },
   /* 1 */ { { G_UB,  G_RT,  G_BLANK}, { G_BLANK, G_FULL, G_BLANK } },
   /* 2 */ { { G_UMB, G_UMB, G_RT   }, { G_LL,  G_LMB, G_LMB } },
   /* 3 */ { { G_UMB, G_UMB, G_RT   }, { G_LMB, G_LMB, G_LR  } },
   /* 4 */ { { G_LL,  G_LB,  G_FULL }, { G_BLANK, G_BLANK, G_FULL } },
   /* 5 */ { { G_FULL,G_UMB, G_UMB  }, { G_LMB, G_LMB, G_LR  } },
   /* 6 */ { { G_LT,  G_UMB, G_UMB  }, { G_LL,  G_LMB, G_LR  } },
   /* 7 */ { { G_UB,  G_UB,  G_RT   }, { G_BLANK, G_BLANK, G_FULL } },
   /* 8 */ { { G_LT,  G_UMB, G_RT   }, { G_LL,  G_LMB, G_LR  } },
   /* 9 */ { { G_LT,  G_UMB, G_RT   }, { G_BLANK, G_BLANK, G_FULL } }
};

/* Cuadros del spinner: | / - \ */
static const uint8_t dspSpinGlyph[DSP_SPIN_FRAMES][8] = {
   { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 },
   { 0x01, 0x02, 0x02, 0x04, 0x04, 0x08, 0x08, 0x10 },
   { 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00, 0x00 },
   { 0x10, 0x08, 0x08, 0x04, 0x04, 0x02, 0x02, 0x01 }
};

/*==================[internal functions definition: utilidades]==============*/

static TickType_t dspMs( uint32_t ms )
{
   TickType_t t = pdMS_TO_TICKS( ms );

   return ( t > 0 ) ? t : 1;
}

/* Ticks que faltan para 'deadline' (0 si ya paso). Seguro ante el desborde. */
static TickType_t dspUntil( TickType_t now, TickType_t deadline )
{
   int32_t d = ( int32_t )( deadline - now );

   return ( d > 0 ) ? ( TickType_t )d : 0;
}

static dspView_t dspViewOf( uint8_t actual )
{
   switch( actual ) {
   case SELECTING:       return VIEW_MENU;
   case SELECT_PLAYERS:
   case SELECT_CARDS:    return VIEW_EDIT;
   case START:           return VIEW_LOADING;
   default:              return VIEW_NONE;
   }
}

static const char* dspLabelOf( uint8_t index )
{
   return dspLabel[ ( index < NUM_STATES ) ? index : 0 ];
}

static void dspLineClear( char* line )
{
   memset( line, ' ', DSP_COLS );
   line[DSP_COLS] = '\0';
}

static void dspLinePut( char* line, uint8_t col, const char* text )
{
   while( *text != '\0' && col < DSP_COLS ) {
      line[col++] = *text++;
   }
}

static void dspLineCenter( char* line, const char* text )
{
   size_t n = strlen( text );

   if( n > DSP_COLS ) {
      n = DSP_COLS;
   }
   dspLinePut( line, ( uint8_t )( ( DSP_COLS - n ) / 2 ), text );
}

/*==================[internal functions definition: dibujo]==================*/

/* Carga un juego de caracteres propios en la CGRAM si no es el que ya esta. */
static bool_t dspLoadGlyphs( dspGlyphs_t set )
{
   const uint8_t ( *src )[8] = ( set == GLYPHS_DIGITS ) ? dspDigitGlyph : dspSpinGlyph;
   uint8_t count = ( set == GLYPHS_DIGITS ) ? G_COUNT : DSP_SPIN_FRAMES;
   bool_t ok = TRUE;
   uint8_t i;

   if( dsp.glyphs == set ) {
      return TRUE;
   }

   dsp.glyphs = GLYPHS_NONE;           /* mientras carga, la CGRAM no es de nadie */
   for( i = 0; i < count; i++ ) {
      ok &= i2cLcdCreateChar( i, src[i] );
   }
   if( ok ) {
      dsp.glyphs = set;
   }
   return ok;
}

/* Fila 0 del menu: '>' fijo y la opcion elegida, que titila. */
static bool_t dspDrawMenuSelected( void )
{
   char line[DSP_COLS + 1];

   dspLineClear( line );
   line[0] = '>';
   if( dsp.blinkOn ) {
      dspLinePut( line, 2, dspLabelOf( dsp.snap.selected ) );
   }
   return i2cLcdPrintLine( 0, line );
}

static bool_t dspDrawMenu( void )
{
   char line[DSP_COLS + 1];
   uint8_t next = ( uint8_t )( ( dsp.snap.selected + 1 ) % NUM_STATES );   /* la que sigue con DOWN */
   bool_t ok = dspDrawMenuSelected();

   dspLineClear( line );
   dspLinePut( line, 2, dspLabelOf( next ) );
   ok &= i2cLcdPrintLine( 1, line );
   return ok;
}

/* Nombre de la opcion que se esta editando (precarga los digitos). */
static bool_t dspDrawEditTitle( void )
{
   char l0[DSP_COLS + 1];
   char l1[DSP_COLS + 1];
   bool_t ok = dspLoadGlyphs( GLYPHS_DIGITS );

   dspLineClear( l0 );
   dspLineClear( l1 );
   dspLineCenter( l0, ( dsp.snap.actual == SELECT_CARDS ) ? DISPLAY_TEXT_CARDS : DISPLAY_TEXT_PLAYERS );
   l1[0]            = '-';
   l1[DSP_COLS - 1] = '+';

   ok &= i2cLcdPrintLine( 0, l0 );
   ok &= i2cLcdPrintLine( 1, l1 );
   return ok;
}

/* '-'  NUMERO GRANDE DE 2 FILAS  '+' */
static bool_t dspDrawEditNumber( void )
{
   char    row[2][DSP_COLS + 1];
   uint8_t slot0Row[DSP_MAX_SLOT0];
   uint8_t slot0Col[DSP_MAX_SLOT0];
   uint8_t slot0 = 0;
   uint8_t digit[DSP_MAX_DIGITS];
   uint8_t n, i, r, k, z, width, left;
   uint8_t v = dsp.snap.value;
   bool_t ok = dspLoadGlyphs( GLYPHS_DIGITS );

   if( v >= 100 ) {
      digit[0] = ( uint8_t )( v / 100 );
      digit[1] = ( uint8_t )( ( v / 10 ) % 10 );
      digit[2] = ( uint8_t )( v % 10 );
      n = 3;
   } else if( v >= 10 ) {
      digit[0] = ( uint8_t )( v / 10 );
      digit[1] = ( uint8_t )( v % 10 );
      n = 2;
   } else {
      digit[0] = v;
      n = 1;
   }

   dspLineClear( row[0] );
   dspLineClear( row[1] );
   row[DISPLAY_SIGN_ROW][0]            = '-';
   row[DISPLAY_SIGN_ROW][DSP_COLS - 1] = '+';

   width = ( uint8_t )( n * DSP_DIGIT_PITCH - 1 );
   left  = ( uint8_t )( ( DSP_COLS - width ) / 2 );

   for( i = 0; i < n; i++ ) {
      for( r = 0; r < 2; r++ ) {
         for( k = 0; k < DSP_DIGIT_CELLS; k++ ) {
            uint8_t code = dspBigDigit[digit[i]][r][k];
            uint8_t col  = ( uint8_t )( left + i * DSP_DIGIT_PITCH + k );

            if( code == 0 && slot0 < DSP_MAX_SLOT0 ) {
               /* El caracter 0 terminaria el string: se escribe aparte. */
               row[r][col]      = ' ';
               slot0Row[slot0]  = r;
               slot0Col[slot0]  = col;
               slot0++;
            } else {
               row[r][col] = ( char )code;
            }
         }
      }
   }

   ok &= i2cLcdPrintLine( 0, row[0] );
   ok &= i2cLcdPrintLine( 1, row[1] );
   for( z = 0; z < slot0; z++ ) {
      ok &= i2cLcdWriteChar( slot0Row[z], slot0Col[z], G_LB );
   }
   return ok;
}

static bool_t dspDrawLoading( void )
{
   char l0[DSP_COLS + 1];
   char l1[DSP_COLS + 1];
   bool_t ok = dspLoadGlyphs( GLYPHS_SPINNER );

   dspLineClear( l0 );
   dspLineClear( l1 );
   dspLineCenter( l0, DISPLAY_TEXT_LOADING );

   ok &= i2cLcdPrintLine( 0, l0 );
   ok &= i2cLcdPrintLine( 1, l1 );
   ok &= i2cLcdWriteChar( 1, DSP_SPIN_COL, dsp.spinFrame );
   return ok;
}

/* Redibuja toda la pantalla de la vista actual. TRUE si todo entro en la cola. */
static bool_t dspRenderFull( void )
{
   switch( dsp.view ) {
   case VIEW_MENU:    return dspDrawMenu();
   case VIEW_EDIT:    return dsp.showTitle ? dspDrawEditTitle() : dspDrawEditNumber();
   case VIEW_LOADING: return dspDrawLoading();
   default:           return TRUE;
   }
}

/*==================[internal functions definition: logica]==================*/

/* Aplica un snapshot nuevo: elige la vista y reinicia las animaciones. */
static void dspApplySnapshot( const dspSnapshot_t* in, TickType_t now )
{
   bool_t entering = ( !dsp.haveSnap || dsp.snap.actual != in->actual );

   dsp.snap     = *in;
   dsp.haveSnap = TRUE;
   dsp.view     = dspViewOf( in->actual );
   dsp.dirty    = TRUE;

   switch( dsp.view ) {
   case VIEW_MENU:                       /* cada movimiento muestra la opcion al instante */
      dsp.blinkOn   = TRUE;
      dsp.nextBlink = now + dspMs( DISPLAY_BLINK_ON_MS );
      break;

   case VIEW_EDIT:                       /* un boton mientras se ve el titulo lo corta */
      dsp.showTitle = ( entering && DISPLAY_TITLE_MS > 0 ) ? TRUE : FALSE;
      dsp.titleEnd  = now + dspMs( DISPLAY_TITLE_MS );
      break;

   case VIEW_LOADING:
      if( entering ) {
         dsp.spinFrame = 0;
         dsp.nextSpin  = now + dspMs( DISPLAY_SPIN_MS );
      }
      break;

   default:
      break;
   }
}

/* Detecta desconexion / reconexion del LCD. TRUE si esta listo para recibir. */
static bool_t dspPollReady( void )
{
   bool_t ready = i2cLcdIsReady();

   if( !ready ) {
      dsp.glyphs = GLYPHS_NONE;          /* al volver hay que recargar la CGRAM */
   } else if( !dsp.wasReady ) {
      dsp.dirty  = TRUE;                 /* volvio (o recien arranca): redibujar */
      dsp.glyphs = GLYPHS_NONE;
   }
   dsp.wasReady = ready;
   return ready;
}

/* Cuanto puede dormir DisplayTask hasta el proximo evento propio. */
static TickType_t dspComputeWait( TickType_t now, bool_t ready )
{
   TickType_t wait = dspMs( DISPLAY_POLL_MS );
   TickType_t t;

   if( !ready ) {
      return wait;
   }
   if( dsp.dirty ) {
      return dspMs( DISPLAY_RETRY_MS );
   }

   switch( dsp.view ) {
   case VIEW_MENU:     t = dspUntil( now, dsp.nextBlink );  break;
   case VIEW_LOADING:  t = dspUntil( now, dsp.nextSpin );   break;
   case VIEW_EDIT:     t = dsp.showTitle ? dspUntil( now, dsp.titleEnd ) : wait;  break;
   default:            t = wait;  break;
   }
   return ( t < wait ) ? t : wait;
}

/* Avanza lo que este vencido: titileo, spinner o fin del titulo. */
static void dspStepAnimation( TickType_t now )
{
   switch( dsp.view ) {

   case VIEW_MENU:
      if( dspUntil( now, dsp.nextBlink ) == 0 ) {
         dsp.blinkOn   = !dsp.blinkOn;
         dsp.nextBlink = now + dspMs( dsp.blinkOn ? DISPLAY_BLINK_ON_MS : DISPLAY_BLINK_OFF_MS );
         dsp.dirty     = !dspDrawMenuSelected();       /* solo la fila 0 */
      }
      break;

   case VIEW_LOADING:
      if( dspUntil( now, dsp.nextSpin ) == 0 ) {
         dsp.spinFrame = ( uint8_t )( ( dsp.spinFrame + 1 ) % DSP_SPIN_FRAMES );
         dsp.nextSpin  = now + dspMs( DISPLAY_SPIN_MS );
         dsp.dirty     = !i2cLcdWriteChar( 1, DSP_SPIN_COL, dsp.spinFrame );
      }
      break;

   case VIEW_EDIT:
      if( dsp.showTitle && dspUntil( now, dsp.titleEnd ) == 0 ) {
         dsp.showTitle = FALSE;                         /* ahora si, el numero */
         dsp.dirty     = TRUE;
      }
      break;

   default:
      break;
   }
}

static void DisplayTask( void* pvParameters )
{
   dspSnapshot_t incoming;

   ( void )pvParameters;

   for( ;; ) {
      TickType_t wait = dspComputeWait( xTaskGetTickCount(), dsp.wasReady );

      if( xQueueReceive( dspMailbox, &incoming, wait ) == pdTRUE ) {
         dspApplySnapshot( &incoming, xTaskGetTickCount() );
      }

      if( !dspPollReady() ) {
         continue;                                     /* sin LCD no se manda nada */
      }

      if( dsp.dirty ) {
         dsp.dirty = !dspRenderFull();                 /* si no entro todo, reintenta */
      } else {
         dspStepAnimation( xTaskGetTickCount() );
      }
   }
}

/*==================[external functions definition]==========================*/

bool_t displayInit( void )
{
   if( dspMailbox != NULL ) {
      return TRUE;                                     /* ya inicializado */
   }

   if( !i2cLcdInit() ) {                               /* idempotente */
      return FALSE;
   }

   dspMailbox = xQueueCreate( 1, sizeof( dspSnapshot_t ) );
   if( dspMailbox == NULL ) {
      return FALSE;
   }

   if( xTaskCreate( DisplayTask, "DisplayTask", DISPLAY_TASK_STACK_WORDS, NULL,
                    DISPLAY_TASK_PRIORITY, NULL ) != pdPASS ) {
      vQueueDelete( dspMailbox );
      dspMailbox = NULL;
      return FALSE;
   }

   return TRUE;
}

void updateDisplay( const config_t* config )
{
   dspSnapshot_t s;

   if( config == NULL || dspMailbox == NULL ) {
      return;
   }

   s.actual   = ( uint8_t )config->actual;
   s.selected = ( uint8_t )config->selected;
   s.value    = 0;
   if( config->actual == SELECT_PLAYERS ) {
      s.value = config->players_aux;
   } else if( config->actual == SELECT_CARDS ) {
      s.value = config->cards_aux;
   }

   xQueueOverwrite( dspMailbox, &s );
}

/*==================[end of file]============================================*/