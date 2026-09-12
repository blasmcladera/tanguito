#include buttons.h

//No se cuantos de estos terminemos usando al final, aca se pueden configurar todos.
static button_t button1;
static button_t button2;
static button_t button3;
static button_t button4;
int flagButton1Pressed;
int flagButton2Pressed;
int flagButton3Pressed;
int flagButton4Pressed;

void buttonsInit(void* param){
    buttonInit( &button1,                  // Button structure (object)
        BUTTON1, BUTTON_LOGIC,       // Pin and electrical connection
        50,                          // Button scan time [ms]
        TRUE,                        // checkPressedEvent
        TRUE,                        // checkReleasedEvent
        TRUE,                        // checkHoldPressedEvent
        3000,                        // holdPressedTime [ms]
        button1PressedCallback,    // pressedCallback
        button1ReleasedCallback,   // releasedCallback
        button1HoldPressedCallback // holdPressedCallback
    );

    buttonInit( &button2,                  // Button structure (object)
        BUTTON2, BUTTON_LOGIC,       // Pin and electrical connection
        50,                          // Button scan time [ms]
        TRUE,                        // checkPressedEvent
        TRUE,                        // checkReleasedEvent
        TRUE,                        // checkHoldPressedEvent
        3000,                        // holdPressedTime [ms]
        button2PressedCallback,    // pressedCallback
        button2ReleasedCallback,   // releasedCallback
        button2HoldPressedCallback // holdPressedCallback
    );

    buttonInit( &button3,                  // Button structure (object)
        BUTTON3, BUTTON_LOGIC,       // Pin and electrical connection
        50,                          // Button scan time [ms]
        TRUE,                        // checkPressedEvent
        TRUE,                        // checkReleasedEvent
        TRUE,                        // checkHoldPressedEvent
        3000,                        // holdPressedTime [ms]
        button3PressedCallback,    // pressedCallback
        button3ReleasedCallback,   // releasedCallback
        button3HoldPressedCallback // holdPressedCallback
    );

    buttonInit( &button4,                  // Button structure (object)
        BUTTON4, BUTTON_LOGIC,       // Pin and electrical connection
        50,                          // Button scan time [ms]
        TRUE,                        // checkPressedEvent
        TRUE,                        // checkReleasedEvent
        TRUE,                        // checkHoldPressedEvent
        3000,                        // holdPressedTime [ms]
        button4PressedCallback,    // pressedCallback
        button4ReleasedCallback,   // releasedCallback
        button4HoldPressedCallback // holdPressedCallback
    );
}

button_t* getButtonStart(void* param){
    return &button1;
}

//Handlers boton 1
void button1PressedCallback(void* param)
{
   flagButton1Pressed = TRUE;
}
void button1ReleasedCallback(void* param)
{
   printf("Boton 1 liberado\r\n");
}
void button1HoldPressedCallback(void* param)
{
   printf("Boton 1 presionado durante mas de 3 segundos\r\n");
}

//Handlers boton 2
void button2PressedCallback(void* param)
{
   printf("Boton 2 presionado\r\n");
}
void button2ReleasedCallback(void* param)
{
   printf("Boton 2 liberado\r\n");
}
void button2HoldPressedCallback(void* param)
{
   printf("Boton 2 presionado durante mas de 3 segundos\r\n");
}

//Handlers boton 3
void button3PressedCallback(void* param)
{
   printf("Boton 3 presionado\r\n");
}
void button3ReleasedCallback(void* param)
{
   printf("Boton 3 liberado\r\n");
}
void button3HoldPressedCallback(void* param)
{
   printf("Boton 3 presionado durante mas de 3 segundos\r\n");
}

//Handlers boton 4
void button4PressedCallback(void* param)
{
   printf("Boton 4 presionado\r\n");
}
void button4ReleasedCallback(void* param)
{
   printf("Boton 4 liberado\r\n");
}
void button4HoldPressedCallback(void* param)
{
   printf("Boton 4 presionado durante mas de 3 segundos\r\n");
}