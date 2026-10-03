/*
 *   - Variables de estado (angulo actual y residuo de pasos)
 *   - turnDegrees():  grados de salida -> pasos del motor (no bloqueante)
 *   - Calculos:       degreesPerPlayer, playerAngle, playerForCard, centerAngle
 *   - rotateTo():     ir a un angulo absoluto y esperar (bloqueante)
 *   - API publica y RotatorTask
 */

#include "rotator.h"
#include "stepper.h"
#include "configuration.h"   /* getConfiguration(), dealConfig_t */

#include "FreeRTOS.h"
#include "task.h"
#include "event_groups.h"

/* Orden de reparto en arco de 180:
 *   0: todas las rondas van jugador 0 -> N-1 (al terminar una ronda hay que
 *      volver todo el arco para empezar la siguiente)
 *   1: las rondas pares van 0 -> N-1 y las impares N-1 -> 0 (se ahorra el
 *      viaje de vuelta, pero cambia el orden en que reciben las cartas)
 * En arco de 360 no se usa: simplemente se sigue girando. */
#define ROTATOR_SERPENTINE   0

/*
 * Se asume algo asi en configuration.h:
 *
 * typedef struct {
 *    uint16_t arcDegrees;       // 180 o 360
 *    uint8_t  players;          // cantidad de jugadores
 *    uint8_t  cardsPerPlayer;   // cartas que recibe cada jugador
 * } dealConfig_t;
 *
 * const dealConfig_t * getConfiguration( void );
 */

/* Donde cree el software que esta parado el repartidor, en grados del
 * engranaje de salida. Se actualiza cada vez que termina un movimiento.
 * Es una posicion "comandada": no hay sensor que la confirme. */
static float currentAngleDeg = 0.0f;

/* Los pasos son enteros pero los grados pedidos no siempre son multiplo de
 * un paso. Ejemplo: si se piden 10.4 pasos se dan 10 y sobran 0.4.
 * Ese 0.4 se guarda aca y se suma en el proximo movimiento, asi el error
 * no se va acumulando con las repeticiones. */
static float stepResidue = 0.0f;

/*
 * Gira el engranaje de salida "Degrees" grados (positivo = FORWARD,
 * negativo = REVERSE). Es no bloqueante: solo ordena el movimiento al
 * stepper; usar stepperIsBusy() para saber cuando termino.
 *
 * Devuelve TRUE si el pedido fue aceptado, FALSE si no se pudo (por ejemplo
 * porque el stepper todavia se estaba moviendo).
 */
static bool_t turnDegrees( float Degrees ){
   /* Cuantos grados del EJE DEL MOTOR avanza cada (micro)paso */
   const float degreesPerStep = STEPPER_STEP_ANGLE_DEG / STEPPER_MICROSTEPS;
   float stepsF;                /* pasos exactos, con decimales */
   int32_t stepsRounded;        /* pasos enteros que realmente se dan (con signo) */
   uint32_t steps;              /* cantidad de pasos sin signo para stepperMove */
   stepperDirection_t dir;      /* ajustar al tipo real del enum de stepper.h */

   /* No se puede pedir un movimiento nuevo si hay uno en curso */
   if( stepperIsBusy() ) {
      return FALSE;
   }

   /* Grados de la salida -> grados del eje del motor (por el engranaje)
    * -> cantidad de pasos. Se suma el residuo del movimiento anterior. */
   stepsF = (Degrees * ROTATOR_GEAR_RATIO) / degreesPerStep + stepResidue;

   /* Redondeo al entero mas cercano respetando el signo
    * (+0.5 si es positivo, -0.5 si es negativo, y el cast trunca) */
   stepsRounded = (int32_t)( (stepsF >= 0.0f) ? (stepsF + 0.5f)
                                              : (stepsF - 0.5f) );

   /* Si el movimiento es menor a medio paso no hay nada para mover:
    * se guarda todo como residuo y se cuenta como exito */
   if( stepsRounded == 0 ) {
      stepResidue = stepsF;
      return TRUE;
   }

   /* El signo decide la direccion; la cantidad de pasos va siempre positiva */
   if( stepsRounded < 0 ) {
      dir = STEPPER_DIRECTION_REVERSE;
      steps = (uint32_t)(-stepsRounded);
   } else {
      dir = STEPPER_DIRECTION_FORWARD;
      steps = (uint32_t)stepsRounded;
   }

   /* Primero direccion, despues pasos. Si alguno falla se aborta y el
    * residuo NO se toca (el movimiento no ocurrio). */
   if( !stepperSetDirection( dir ) ) {
      return FALSE;
   }

   if( !stepperMove( steps ) ) {
      return FALSE;
   }

   /* Lo que sobro por redondear queda para el proximo movimiento */
   stepResidue = stepsF - (float)stepsRounded;
   return TRUE;
}

/*
 * Grados entre un jugador y el siguiente.
 *
 * 360: los jugadores se reparten parejo en toda la circunferencia.
 *      Se divide por N (y no por N-1) porque el jugador "N" caeria en
 *      la misma posicion que el 0.
 *        ej: 4 jugadores -> 90 grados -> posiciones 0, 90, 180, 270
 *
 * 180: el primer jugador queda en 0 y el ultimo en 180 (los extremos del
 *      arco), por eso se divide por N-1.
 *        ej: 3 jugadores -> 90 grados -> posiciones 0, 90, 180
 *
 * Con 1 jugador (o 0) no hay a donde moverse: devuelve 0.
 */
static float degreesPerPlayer( const dealConfig_t * cfg ){
   if( cfg->players <= 1 ) {
      return 0.0f;
   }
   if( cfg->arcDegrees >= 360 ) {
      return 360.0f / (float)cfg->players;
   }
   return (float)cfg->arcDegrees / (float)(cfg->players - 1);
}

/* Angulo absoluto (0 = primer jugador) al que esta el jugador "player"
 * (player va de 0 a players-1) */
static float playerAngle( const dealConfig_t * cfg, uint8_t player ){
   return (float)player * degreesPerPlayer( cfg );
}

/*
 * A que jugador le toca la carta numero "card" (card va de 0 a total-1).
 *
 * Se reparte de a una carta por jugador y por ronda:
 *   ronda    = card / jugadores   (cuantas vueltas completas ya se dieron)
 *   posicion = card % jugadores   (que lugar dentro de la ronda)
 *
 *   ej: 3 jugadores -> cartas 0,1,2 van a los jugadores 0,1,2 (ronda 0)
 *                      cartas 3,4,5 van a los jugadores 0,1,2 (ronda 1)
 *
 * Con ROTATOR_SERPENTINE (solo arco < 360) las rondas impares se recorren al
 * reves, asi el repartidor no tiene que volver al principio.
 */
static uint8_t playerForCard( const dealConfig_t * cfg, uint32_t card ){
   uint32_t round = card / cfg->players;
   uint32_t k     = card % cfg->players;

#if ROTATOR_SERPENTINE
   if( cfg->arcDegrees < 360 && (round & 1u) ) {
      return (uint8_t)( cfg->players - 1u - k );
   }
#endif
   (void)round;   /* evita warning de variable sin usar si SERPENTINE = 0 */
   return (uint8_t)k;
}

/* Posicion de reposo al terminar de repartir:
 * centro del arco en 180 (queda mirando al medio de la mesa),
 * o 0 si es vuelta completa (no hay un "centro" que tenga sentido) */
static float centerAngle( const dealConfig_t * cfg ){
   return (cfg->arcDegrees >= 360) ? 0.0f : (float)cfg->arcDegrees / 2.0f;
}

/*
 * Lleva el repartidor al angulo absoluto "targetDeg" y NO vuelve hasta que
 * llego. Convierte el angulo absoluto en un movimiento relativo:
 *      delta = destino - posicion actual
 *
 * En arco de 360 se puede girar para cualquier lado, asi que el delta se
 * normaliza a (-180, 180] para tomar el camino mas corto.
 *   ej: de 270 a 0: delta = -270 -> se convierte en +90 (mas corto)
 * En arco de 180 hay un tope mecanico y el delta se usa tal cual.
 *
 * Devuelve FALSE si el stepper rechazo el movimiento.
 */
static bool_t rotateTo( float targetDeg, uint16_t arcDegrees ){
   float delta = targetDeg - currentAngleDeg;

   if( arcDegrees >= 360 ) {
      while( delta >  180.0f ) { delta -= 360.0f; }
      while( delta <= -180.0f ) { delta += 360.0f; }
   }

   /* Se ordena el movimiento (no bloqueante) */
   if( !turnDegrees( delta ) ) {
      return FALSE;
   }

   /* Se espera a que el stepper termine. vTaskDelay cede la CPU a otras
    * tasks mientras tanto. Si el stepper ya avisa el final con un bit de
     event group esto lo tenemos que cambiar asi queda mejor y no vuelve a esta tarea
     cada literalmente 1ms. */
   while( stepperIsBusy() ) {
      vTaskDelay( pdMS_TO_TICKS( 1 ) );
   }

   /* Recien ahora se da por valida la nueva posicion */
   currentAngleDeg = targetDeg;
   return TRUE;
}

/* Estos son las funciones publicas para cualquiera que las quiera usar
*/

float rotatorGetAngle( void ){
   return currentAngleDeg;
}

/* Sirve para decirle al modulo donde esta parado realmente
 * (ej: al arrancar, o despues de un homing) */
void rotatorSetAngle( float Degrees ){
   currentAngleDeg = Degrees;
}

/*
 * Task principal del repartidor. Vive en un loop infinito:
 * duerme hasta que llega DEAL_START, reparte un mazo completo y vuelve a
 * dormir esperando el siguiente pedido.
 */
void RotatorTask( void * params ){
   const dealConfig_t * cfg;   /* configuracion de este reparto */
   uint32_t totalCards;        /* cartas a repartir en total */
   uint32_t card;              /* indice de la carta actual (0..total-1) */

   (void)params;

   while( TRUE ){
      /* Espera bloqueada hasta que alguien setee DEAL_START.
       * pdTRUE (1ro): limpia el bit al despertar, asi no dispara dos veces.
       * pdTRUE (2do): espera TODOS los bits pedidos (aca es uno solo). */
      xEventGroupWaitBits( getMainEventGroup(), DEAL_START, pdTRUE, pdTRUE, portMAX_DELAY );

      /* Se lee la configuracion al inicio de cada reparto */
      cfg = getConfiguration();
      totalCards = (uint32_t)cfg->players * cfg->cardsPerPlayer;

      for( card = 0; card < totalCards; card++ ) {
         uint8_t player = playerForCard( cfg, card );

         /* 1) Posicionar: ir al angulo del jugador y esperar a llegar */
         if( !rotateTo( playerAngle( cfg, player ), cfg->arcDegrees ) ) {
            break;   /* TODO: manejo de error (reintentar, avisar, etc.) */
         }

         /* 2) Expulsar: pedir que salga la carta y esperar a que termine */
         xEventGroupSetBits( getMainEventGroup(), EJECT_START );
         xEventGroupWaitBits( getMainEventGroup(), EJECT_DONE, pdTRUE, pdTRUE, portMAX_DELAY );
      }

      /* Terminado (o abortado): volver al reposo y avisar al main */
      rotateTo( centerAngle( cfg ), cfg->arcDegrees );
      xEventGroupSetBits( getMainEventGroup(), DEAL_DONE );
   }
}