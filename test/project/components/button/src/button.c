#include "button.h"
#include "event.h"
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"

//No se cuantos de estos terminemos usando al final, aca se pueden configurar todos.
static button_t buttonEnter;
static button_t buttonUp;
static button_t buttonDown;
static button_t buttonBack;

// --- BOTÓN ENTER (TEC1) ---
void buttonEnterPressedCallback(void* param)
{
    // Setea el bit de ENTER de forma segura para FreeRTOS
    // (Usa xEventGroupSetBitsFromISR si estos callbacks se ejecutan dentro de una interrupción)
    xEventGroupSetBits(getButtonEventGroup(), PRESSED_ENTER);
}

void buttonEnterReleasedCallback(void* param)
{
    // No hace nada
}

void buttonEnterHoldPressedCallback(void* param)
{
    // No hace nada
}

// --- BOTÓN UP (TEC2) ---
void buttonUpPressedCallback(void* param)
{
    xEventGroupSetBits(getButtonEventGroup(), PRESSED_UP);
}

void buttonUpReleasedCallback(void* param)
{
    // No hace nada
}

void buttonUpHoldPressedCallback(void* param)
{
    // No hace nada
}

// --- BOTÓN DOWN (TEC3) ---
void buttonDownPressedCallback(void* param)
{
    xEventGroupSetBits(getButtonEventGroup(), PRESSED_DOWN);
}

void buttonDownReleasedCallback(void* param)
{
    // No hace nada
}

void buttonDownHoldPressedCallback(void* param)
{
    // No hace nada
}

// --- BOTÓN BACK (TEC4) ---
void buttonBackPressedCallback(void* param)
{
    xEventGroupSetBits(getButtonEventGroup(), PRESSED_BACK);
}

void buttonBackReleasedCallback(void* param)
{
    // No hace nada
}

void buttonBackHoldPressedCallback(void* param)
{
    // No hace nada
}

void ButtonTask(void *pvParameters) {

    while (1) {
        // Actualizas la máquina de estados de cada botón cada 50ms
        buttonFsmUpdate(&buttonEnter);
        buttonFsmUpdate(&buttonUp);
        buttonFsmUpdate(&buttonDown);
        buttonFsmUpdate(&buttonBack);

        // Esperar el tiempo de refresco configurado (50 ms)
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void buttonsInit(){
    buttonInit( &buttonEnter,                 // Button structure (object)
        BUTTON_ENTER, BUTTON_LOGIC,      // Pin and electrical connection
        50,                                // Button scan time [ms]
        TRUE,                              // checkPressedEvent
        TRUE,                              // checkReleasedEvent
        TRUE,                              // checkHoldPressedEvent
        3000,                              // holdPressedTime [ms]
        buttonEnterPressedCallback,      // pressedCallback
        buttonEnterReleasedCallback,     // releasedCallback
        buttonEnterHoldPressedCallback   // holdPressedCallback
    );

    buttonInit( &buttonUp,                    // Button structure (object)
        BUTTON_UP, BUTTON_LOGIC,         // Pin and electrical connection
        50,                                // Button scan time [ms]
        TRUE,                              // checkPressedEvent
        TRUE,                              // checkReleasedEvent
        TRUE,                              // checkHoldPressedEvent
        3000,                              // holdPressedTime [ms]
        buttonUpPressedCallback,         // pressedCallback
        buttonUpReleasedCallback,        // releasedCallback
        buttonUpHoldPressedCallback      // holdPressedCallback
    );

    buttonInit( &buttonDown,                  // Button structure (object)
        BUTTON_DOWN, BUTTON_LOGIC,       // Pin and electrical connection
        50,                                // Button scan time [ms]
        TRUE,                              // checkPressedEvent
        TRUE,                              // checkReleasedEvent
        TRUE,                              // checkHoldPressedEvent
        3000,                              // holdPressedTime [ms]
        buttonDownPressedCallback,       // pressedCallback
        buttonDownReleasedCallback,      // releasedCallback
        buttonDownHoldPressedCallback    // holdPressedCallback
    );

    buttonInit( &buttonBack,                  // Button structure (object)
        BUTTON_BACK, BUTTON_LOGIC,       // Pin and electrical connection
        50,                                // Button scan time [ms]
        TRUE,                              // checkPressedEvent
        TRUE,                              // checkReleasedEvent
        TRUE,                              // checkHoldPressedEvent
        3000,                              // holdPressedTime [ms]
        buttonBackPressedCallback,       // pressedCallback
        buttonBackReleasedCallback,      // releasedCallback
        buttonBackHoldPressedCallback    // holdPressedCallback
    );
    
    xTaskCreate(
      ButtonTask,           // Función de la tarea
      "ButtonTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Parámetros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );
}