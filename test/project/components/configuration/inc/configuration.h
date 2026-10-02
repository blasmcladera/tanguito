#ifndef _CONFIGURATION_H_
#define _CONFIGURATION_H_

typedef enum {
   SELECT_PLAYERS,
   SELECT_CARDS,
   START,
   SELECTING
} state_t;

#define NUM_STATES 3

typedef struct {
   state_t actual;
   state_t selected;
   uint8_t players;
   uint8_t players_aux;
   uint8_t cards;
   uint8_t cards_aux;
} config_t;





#endif