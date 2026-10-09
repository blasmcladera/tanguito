/*
 * display.h
 *
 * Vista en el LCD de la configuracion del sistema (componente configuration).
 * Usa la libreria i2c_lcd (components/lcd).
 *
 * PANTALLAS (segun config->actual):
 *
 *   SELECTING                    SELECT_PLAYERS / SELECT_CARDS      START
 *   +----------------+           +----------------+                +----------------+
 *   |> Jugadores     |  <- fila  |  ##  ##  ##    |                |    Cargando    |
 *   |  Cartas        |   0 titila|- ##  ##  ##  + |                |       |        |
 *   +----------------+           +----------------+                +----------------+
 *     opcion actual (titila)       numero grande de 2 filas          spinner girando
 *     y abajo la que sigue         con '-' a la izquierda y '+'
 *     si se aprieta DOWN           a la derecha
 *
 *  - En SELECTING la opcion elegida se marca con '>' fijo y su texto titila.
 *    (Un LCD de caracteres no tiene brillo por renglon, por eso se titila.)
 *  - Al entrar a editar Jugadores / Cartas se muestra el nombre de la opcion
 *    DISPLAY_TITLE_MS milisegundos y despues el numero grande. Con 16x2 no hay
 *    lugar para ambos a la vez. Se desactiva con -DDISPLAY_TITLE_MS=0.
 *  - En START queda el spinner hasta que se vuelva a llamar a updateDisplay()
 *    con otro estado. Esto es a proposito: configuration.c pasa a SELECTING
 *    enseguida, pero el spinner debe seguir mientras el sistema mezcla/reparte;
 *    se corta cuando arranca la siguiente configuracion (CONFIGURATION_START).
 *
 * USO
 *    displayInit();                 // lo llama configInit(); crea tarea + LCD
 *    ...
 *    updateDisplay(&config);        // tras cada cambio de estado o de valor
 *
 * updateDisplay() NO bloquea, NO toca el bus I2C y no consume stack: solo deja
 * una "foto" del estado en un buzon de 1 lugar (la ultima gana). Quien dibuja,
 * titila y anima es DisplayTask. No se puede llamar desde una ISR.
 *
 * Solo ASCII (el LCD no tiene tildes ni enie). Requiere LCD de >= 15 columnas
 * y >= 2 filas (16x2, 20x2, 20x4...).
 */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#include <stdint.h>
#include "sapi_datatypes.h"
#include "configuration.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==================[configuracion]==========================================*/

/* Textos (ASCII, maximo ~14 caracteres para que entren con el '> '). */
#ifndef DISPLAY_TEXT_PLAYERS
#define DISPLAY_TEXT_PLAYERS      "Jugadores"
#endif
#ifndef DISPLAY_TEXT_CARDS
#define DISPLAY_TEXT_CARDS        "Cartas"
#endif
#ifndef DISPLAY_TEXT_START
#define DISPLAY_TEXT_START        "Iniciar"
#endif
#ifndef DISPLAY_TEXT_LOADING
#define DISPLAY_TEXT_LOADING      "Cargando"
#endif

/* Titileo de la opcion elegida: visible / oculta (ms). */
#ifndef DISPLAY_BLINK_ON_MS
#define DISPLAY_BLINK_ON_MS       600
#endif
#ifndef DISPLAY_BLINK_OFF_MS
#define DISPLAY_BLINK_OFF_MS      250
#endif

/* Un paso del spinner (ms). */
#ifndef DISPLAY_SPIN_MS
#define DISPLAY_SPIN_MS           120
#endif

/* Nombre de la opcion al entrar a editar, antes del numero (0 = no mostrar). */
#ifndef DISPLAY_TITLE_MS
#define DISPLAY_TITLE_MS          800
#endif

/* Fila de los '-' y '+'. 1 queda mas cerca del centro vertical del numero. */
#ifndef DISPLAY_SIGN_ROW
#define DISPLAY_SIGN_ROW          1
#endif

/* Cada cuanto se fija si el LCD volvio tras desconectarse (ms). */
#ifndef DISPLAY_POLL_MS
#define DISPLAY_POLL_MS           200
#endif

/* Reintento de dibujo si la cola del LCD estaba llena (ms). */
#ifndef DISPLAY_RETRY_MS
#define DISPLAY_RETRY_MS          50
#endif

#ifndef DISPLAY_TASK_STACK_WORDS
#define DISPLAY_TASK_STACK_WORDS  192          /* palabras de 32 bits = 768 B */
#endif

#ifndef DISPLAY_TASK_PRIORITY
#define DISPLAY_TASK_PRIORITY     ( tskIDLE_PRIORITY + 1 )
#endif

/*==================[funciones publicas]=====================================*/

/*
 * Inicia el LCD (i2cLcdInit, idempotente), el buzon y la tarea.
 * Llamar antes de vTaskStartScheduler() o desde una tarea. FALSE si no hubo
 * heap; en ese caso updateDisplay() queda como un no-op.
 */
bool_t displayInit( void );

/*
 * Refleja 'config' en el LCD. Mira solo: actual, selected, players_aux (en
 * SELECT_PLAYERS) y cards_aux (en SELECT_CARDS), que es el valor que se esta
 * editando. Llamar desde la tarea que modifica 'config'.
 */
void updateDisplay( const config_t* config );

#ifdef __cplusplus
}
#endif

#endif /* _DISPLAY_H_ */