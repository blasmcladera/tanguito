#ifndef _BUTTON_H_
#define _BUTTON_H_

#include "sapi.h"

//Podríamos renombrarlos por su uso, por ejemplo BUTTON_START para el start.
#define BUTTON_ENTER TEC1
#define BUTTON_UP    TEC2
#define BUTTON_DOWN  TEC3
#define BUTTON_BACK  TEC4
//Input Pull-Up
#define BUTTON_LOGIC BUTTON_ONE_IS_UP

void buttonsInit(void* param);

// Prototipos BUTTON_ENTER (TEC1)
void buttonEnterPressedCallback(void* param);
void buttonEnterReleasedCallback(void* param);
void buttonEnterHoldPressedCallback(void* param);

// Prototipos BUTTON_UP (TEC2)
void buttonUpPressedCallback(void* param);
void buttonUpReleasedCallback(void* param);
void buttonUpHoldPressedCallback(void* param);

// Prototipos BUTTON_DOWN (TEC3)
void buttonDownPressedCallback(void* param);
void buttonDownReleasedCallback(void* param);
void buttonDownHoldPressedCallback(void* param);

// Prototipos BUTTON_BACK (TEC4)
void buttonBackPressedCallback(void* param);
void buttonBackReleasedCallback(void* param);
void buttonBackHoldPressedCallback(void* param);

void buttonsInit();

#endif