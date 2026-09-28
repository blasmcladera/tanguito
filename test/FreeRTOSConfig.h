/*
 * FreeRTOSConfig.h -- configuracion para EDU-CIAA (NXP LPC4337, Cortex-M4F)
 *                     Port: FreeRTOS-Kernel/portable/GCC/ARM_CM4F
 *
 * Este archivo cumple, en el mundo Cortex-M, el mismo rol que tenia tu
 * FreeRTOSVariant.h en AVR: le dice al port de donde saca el tick y como
 * se comporta el kernel. La diferencia es que aca NO elegis la fuente del
 * tick (WDT vs Timer0): el port ARM_CM4F usa el SysTick del propio core
 * automaticamente, a traves de una funcion "weak" (vPortSetupTimerInterrupt)
 * que ya viene resuelta en port.c. Solo le das la frecuencia de reloj real
 * y la frecuencia de tick que queres.
 *
 * IMPORTANTE: configCPU_CLOCK_HZ tiene que coincidir con el clock real del
 * nucleo M4 en el momento en que arranca el scheduler. En proyectos basados
 * en LPCOpen/CMSIS esto normalmente ya esta resuelto por la variable global
 * SystemCoreClock, que se actualiza en el arranque con SystemCoreClockUpdate().
 * Por eso abajo se referencia esa variable en vez de un numero fijo: si
 * cambias el arbol de clocks (CGU) del LPC4337, este archivo no se desactualiza.
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

/* La declara y actualiza el startup/CMSIS de LPCOpen (system_LPC43xx.c). */
extern uint32_t SystemCoreClock;

/* ---------------------------------------------------------------------------
 * Reloj y tick
 * ------------------------------------------------------------------------- */
#define configCPU_CLOCK_HZ                 ( SystemCoreClock )
#define configTICK_RATE_HZ                 ( ( TickType_t ) 1000 )  /* 1 ms por tick */
#define configUSE_16_BIT_TICKS             0   /* Cortex-M4 es de 32 bits, no hace falta ahorrar */

/* ---------------------------------------------------------------------------
 * Comportamiento del scheduler
 * ------------------------------------------------------------------------- */
#define configUSE_PREEMPTION               1
#define configUSE_TIME_SLICING             1
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 1
#define configUSE_TICKLESS_IDLE            0
#define configIDLE_SHOULD_YIELD            1
#define configMAX_PRIORITIES               ( 5 )
#define configMINIMAL_STACK_SIZE           ( ( unsigned short ) 128 )  /* en words, no bytes */
#define configTOTAL_HEAP_SIZE              ( ( size_t ) ( 32 * 1024 ) )
#define configMAX_TASK_NAME_LEN            16

/* ---------------------------------------------------------------------------
 * Prioridades de interrupcion del NVIC
 *
 * El LPC43xx implementa 3 bits de prioridad en el NVIC (8 niveles: 0-7).
 * Confirmalo en tu CMSIS device header (__NVIC_PRIO_BITS en LPC43xx.h) antes
 * de dar esto por sentado en tu placa puntual.
 * ------------------------------------------------------------------------- */
#define configPRIO_BITS                    3
#define configLIBRARY_LOWEST_INTERRUPT_PRIORITY       7
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY  5

#define configKERNEL_INTERRUPT_PRIORITY \
    ( configLIBRARY_LOWEST_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )
#define configMAX_SYSCALL_INTERRUPT_PRIORITY \
    ( configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << ( 8 - configPRIO_BITS ) )

/* ---------------------------------------------------------------------------
 * Hooks (los implementas vos en tu main.c, igual que antes)
 * ------------------------------------------------------------------------- */
#define configUSE_IDLE_HOOK                1
#define configUSE_MALLOC_FAILED_HOOK       1
#define configCHECK_FOR_STACK_OVERFLOW     2   /* metodo 2: mas robusto que 1 */
#define configUSE_TICK_HOOK                0

/* ---------------------------------------------------------------------------
 * Sincronizacion / utilidades habilitadas
 * ------------------------------------------------------------------------- */
#define configUSE_MUTEXES                  1
#define configUSE_RECURSIVE_MUTEXES        1
#define configUSE_COUNTING_SEMAPHORES      1
#define configUSE_QUEUE_SETS               0
#define configQUEUE_REGISTRY_SIZE          8
#define configUSE_TRACE_FACILITY           0
#define configGENERATE_RUN_TIME_STATS      0

/* ---------------------------------------------------------------------------
 * Funciones opcionales de la API que queres poder usar
 * ------------------------------------------------------------------------- */
#define INCLUDE_vTaskPrioritySet           1
#define INCLUDE_uxTaskPriorityGet          1
#define INCLUDE_vTaskDelete                1
#define INCLUDE_vTaskSuspend               1
#define INCLUDE_vTaskDelayUntil            1
#define INCLUDE_vTaskDelay                 1
#define INCLUDE_xTaskGetSchedulerState     1
#define INCLUDE_xTimerPendFunctionCall     0

/* Cortex-M4F: la FPU se guarda por tarea automaticamente (lazy stacking) si
 * la tarea usa instrucciones de punto flotante; no hay macro de config para
 * "activarla" como en otros ports. Lo que si es obligatorio es compilar con
 * -mfpu=fpv4-sp-d16 -mfloat-abi=hard (o softfp), porque port.c de ARM_CM4F
 * verifica __VFP_FP__ y no compila sin eso. */

/* ---------------------------------------------------------------------------
 * Declaraciones que implementas en tu main.c, igual que en tu variante AVR
 * ------------------------------------------------------------------------- */
void vApplicationIdleHook( void );
void vApplicationMallocFailedHook( void );
void vApplicationStackOverflowHook( void *xTask, char *pcTaskName );

#endif /* FREERTOS_CONFIG_H */