#include "event.h"
#include "configuration.h"
#include "FreeRTOS.h"
#include "task.h"
#include "display.h"
#include <stdint.h>

static config_t config; 

static void update(uint8_t event){
   switch (config.actual){
      case SELECTING:
         switch(event){
            case PRESSED_ENTER:  config.actual=config.selected; break;
            case PRESSED_UP:     config.selected = (config.selected - 1) % NUM_STATES; break;
            case PRESSED_DOWN:   config.selected = (config.selected + 1) % NUM_STATES; break;
            default: break;
         } break;
      case SELECT_PLAYERS:
         switch(event){
            case PRESSED_ENTER:  
               config.players = config.players_aux;
               config.players_aux = 0;
               config.actual = SELECTING;
               break;
            case PRESSED_UP:     config.players_aux++; break;
            case PRESSED_DOWN:   config.players_aux--; break;
            case PRESSED_BACK:
               config.players_aux = 0;
               config.actual = SELECTING;
               break;
            default: break;
         } break;
      case SELECT_CARDS:
         switch(event){
            case PRESSED_ENTER:  
               config.cards = config.cards_aux;
               config.cards_aux = 0;
               config.actual = SELECTING;
               break;
            case PRESSED_UP:     config.cards_aux++; break;
            case PRESSED_DOWN:   config.cards_aux--; break;
            case PRESSED_BACK:
               config.cards_aux = 0;
               config.actual = SELECTING;
               break;
            default: break;
         } break;
      case START:
      default:
         break;
   }
}   

void ConfigurationTask(void * params){
   
while (1){
   xEventGroupWaitBits(getMainEventGroup(),CONFIGURATION_START, pdTRUE, pdTRUE, portMAX_DELAY);
   updateDisplay(&config);   // muestra el menu (y corta el spinner de la vuelta anterior)
   do {
      EventBits_t event_bits = xEventGroupWaitBits(getButtonEventGroup(),PRESSED_BUTTON, pdFALSE, pdFALSE, portMAX_DELAY);
      uint8_t event = isEventAndClear(event_bits);
      update(event);
      updateDisplay(&config);   // si el estado paso a START, arranca el spinner
   } while (config.actual != START);
   config.actual = SELECTING;
   config.selected= SELECT_PLAYERS;
   config.players_aux = config.players;
   config.cards_aux = config.cards;
   xEventGroupSetBits(getMainEventGroup(), CONFIGURATION_DONE);
}
}

void configInit(){
   config = (config_t){
    .actual = SELECTING,
    .selected = SELECT_PLAYERS,
    .players = 0,
    .players_aux = 0,
    .cards = 0,
    .cards_aux = 0
   };
   
   displayInit();   // LCD + tarea de la vista

   xTaskCreate(
      ConfigurationTask,           // Funci?n de la tarea
      "ConfigurationTask",         // Nombre de la tarea
      configMINIMAL_STACK_SIZE,
      NULL,               // Par?metros
      tskIDLE_PRIORITY + 1,
      NULL                // Handle
   );




}