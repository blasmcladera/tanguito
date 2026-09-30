#include <stdlib.h>
#include "sapi.h"
#include "FreeRTOS.h"
#include "task.h"

#define ENA      PWM0   // T_FIL1, enable del motor A
#define ENB      PWM3   // T_FIL2, enable del motor B
 
#define IN1      GPIO0  // siempre en 1
#define IN2      GPIO1  // o tmb conectado a GND
#define IN3      GPIO2  // ídem
#define IN4      GPIO3  // siempre en 1
 
#define VEL_MAX  255    // 100%, prendidos a máxima potencia
#define VEL_OFF  0      // 0%, apagados
 
#define T_MIN_MS 1000   // duración mínima de cada turno de los motores 
#define T_MAX_MS 3000   // duración máxima de cada turno

void mainTask( void* pvParameters )
{
   while( TRUE ) {
      gpioToggle(CIAA_BOARD_LED);
      vTaskDelay( pdMS_TO_TICKS( 100 ) );
   }
}

int main( void )
{
   boardConfig();
   
   shufflerInit();
   
   xTaskCreate(
      mainTask,           // Función de la tarea
      "mainTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Parámetros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );

   vTaskStartScheduler();


   while( TRUE );
   return 0;
}
