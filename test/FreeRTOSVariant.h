/*
 * FreeRTOSVariant.h  --  configuracion especifica del AVR para el port
 *                        FreeRTOS-Kernel/portable/ThirdParty/GCC/ATmega
 *
 * Este archivo es el que el port espera que TU escribas. Define de donde
 * sale el tick y como se llama el vector de interrupcion que lo genera.
 *
 * La ISR en si NO la escribis vos: esta en port.c del port de ATmega, que
 * hace algo como
 *
 *     ISR( portSCHEDULER_ISR, ISR_NAKED )
 *     {
 *         vPortYieldFromTick();
 *         __asm__ __volatile__ ( "reti" );
 *     }
 *
 * y usa el macro portSCHEDULER_ISR que definimos mas abajo. Lo que te toca
 * es elegir la fuente del tick con portUSE_WDTO o portUSE_TIMER0.
 */

#ifndef FREERTOS_VARIANT_H
#define FREERTOS_VARIANT_H

#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>

/* ---------------------------------------------------------------------------
 * OPCION A (recomendada): tick desde el Watchdog Timer
 *
 * Ventaja grande: Timer0, Timer1 y Timer2 quedan libres para tu aplicacion
 * (PWM, captura, lo que sea). Desventaja: el WDT corre con su oscilador
 * interno de 128 kHz, que no esta calibrado ni compensado por temperatura,
 * asi que el tick tiene un error de varios puntos porcentuales.
 *
 * Periodos validos: WDTO_15MS, WDTO_30MS, WDTO_60MS, WDTO_120MS,
 *                   WDTO_250MS, WDTO_500MS, WDTO_1S, WDTO_2S
 *
 * Los tiempos reales son potencias de dos del oscilador de 128 kHz:
 * WDTO_15MS son en realidad 2048/128000 = 16 ms.
 * ------------------------------------------------------------------------- */
#define portUSE_WDTO            WDTO_15MS
#define portSCHEDULER_ISR       WDT_vect

/* 128000 Hz / (2048 << portUSE_WDTO) ciclos  ==  128000 >> (portUSE_WDTO + 11)
 *
 *   WDTO_15MS  ->  62 Hz   (~16 ms por tick)
 *   WDTO_30MS  ->  31 Hz
 *   WDTO_60MS  ->  15 Hz
 *   WDTO_120MS ->   7 Hz
 *
 * La division es entera, asi que la granularidad es gruesa: pdMS_TO_TICKS(500)
 * con 62 Hz da 31 ticks, o sea ~496 ms reales. Si necesitas precision de
 * milisegundos, usa la OPCION B. */
#define configTICK_RATE_HZ      ( ( TickType_t ) ( ( uint32_t ) 128000 >> ( portUSE_WDTO + 11 ) ) )

/* ---------------------------------------------------------------------------
 * OPCION B: tick desde Timer0 en modo CTC
 *
 * Tick preciso derivado del cristal, a costa de ocupar Timer0. Si venis del
 * mundo Arduino, tene en cuenta que esto rompe millis() y micros().
 * Para usarla, comenta las tres definiciones de la OPCION A y descomenta
 * estas dos. configTICK_RATE_HZ lo elegis vos libremente (200-1000 Hz es
 * razonable; mas alto y el AVR se pasa la vida cambiando de contexto).
 * ------------------------------------------------------------------------- */
/*
#define portUSE_TIMER0
#define portSCHEDULER_ISR       TIMER0_COMPA_vect
#define configTICK_RATE_HZ      ( ( TickType_t ) 1000 )
*/

/* ---------------------------------------------------------------------------
 * Declaraciones que el port necesita ver
 * ------------------------------------------------------------------------- */

/* La implementa port.c segun portUSE_WDTO / portUSE_TIMER0. */
//void prvSetupTimerInterrupt( void );

/* Hooks que implementas en tu main.c (ver el ejemplo). */
void vApplicationIdleHook( void );
void vApplicationMallocFailedHook( void );
//void vApplicationStackOverflowHook( TaskHandle_t xTask, char * pcTaskName );

#endif /* FREERTOS_VARIANT_H */
