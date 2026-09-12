#ifndef _BUTTONS_H_
#define _BUTTONS_H_

#include "sapi.h"

//Podríamos renombrarlos por su uso, por ejemplo BUTTON_START para el start.
#define BUTTON1 TEC1
#define BUTTON2 TEC2
#define BUTTON3 TEC3
#define BUTTON4 TEC4
//Input Pull-Up
#define BUTTON_LOGIC BUTTON_ONE_IS_UP

extern int flagButton1Pressed;
extern int flagButton2Pressed;
extern int flagButton3Pressed;
extern int flagButton4Pressed;

void buttonsInit(void* param);
button_t* getButtonStart(void* param);

// Prototipos boton 1
void button1PressedCallback(void* param);
void button1ReleasedCallback(void* param);
void button1HoldPressedCallback(void* param);

// Prototipos boton 2
void button2PressedCallback(void* param);
void button2ReleasedCallback(void* param);
void button2HoldPressedCallback(void* param);

// Prototipos boton 3
void button3PressedCallback(void* param);
void button3ReleasedCallback(void* param);
void button3HoldPressedCallback(void* param);

// Prototipos boton 4
void button4PressedCallback(void* param);
void button4ReleasedCallback(void* param);
void button4HoldPressedCallback(void* param);

#endif