/* Prueba de 2 motores DC con L298N - EDU-CIAA-NXP (usando sAPI para generar los pwm)
 *
 * Motor 1: IN1 <- T_FIL1 (PWM0)
 * Motor 2: IN4 <- T_FIL2 (PWM3)
 * ENA y ENB puenteados a 5V (siempre habilitados)
 * GND de la EDU-CIAA, del L298N y de la fuente de 12V unidos.
 */

/*#include "sapi.h"

#define MOTOR1   PWM0   // T_FIL1
#define MOTOR2   PWM3   // T_FIL2

#define VEL_MAX  255    // 100% 
#define VEL_MED  127    // ~50%  
#define VEL_OFF  0

/* Test minimo: parpadeo de LED1 SIN usar delay() de la sAPI.
 * Usa un for "vacio" que consume tiempo por CPU, para descartar
 * que el problema sea SysTick/delay() y no boardInit()/GPIO.
 */


/* Delay por software (busy-wait). volatile evita que el compilador
 * optimice el loop y lo borre entero. */
/*static void delay_mock(volatile uint32_t count)
{
   while (count--) {
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");
      __asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");__asm__ volatile ("nop");
   }
}

int main(void)
{
   boardConfig();

   // Habilita los timers (SCT) en modo PWM 
   pwmInit(0, PWM_ENABLE);

   // Habilita cada salida PWM 
   pwmInit(MOTOR1, PWM_ENABLE_OUTPUT);
   pwmInit(MOTOR2, PWM_ENABLE_OUTPUT);

   while (1) {
      // Motor 1 ON, Motor 2 OFF 
      pwmWrite(MOTOR1, VEL_MED);
      //pwmWrite(MOTOR2, VEL_OFF);
      delay(2000);
      //delay_mock(30000000);   // ajustar segun lo que tarde 

      // Motor 1 OFF, Motor 2 ON 
      pwmWrite(MOTOR1, VEL_OFF);
      //pwmWrite(MOTOR2, VEL_MED);
      //delay(2000);
      delay_mock(30000000);   // ajustar segun lo que tarde 
   }

   return 0;
}*/

/* Test minimo: parpadeo de LED1 SIN usar delay() de la sAPI.
 * Usa un for "vacio" que consume tiempo por CPU, para descartar
 * que el problema sea SysTick/delay() y no boardInit()/GPIO.
 */

#include "sapi.h"

int main(void)
{
   boardConfig();
   tickInit(1);   // SysTick cada 1 ms

   while (1) {
      gpioWrite(LED1, ON);
      delay(2000);


      gpioWrite(LED1, OFF);
      delay(1000);
   }

   return 0;
}