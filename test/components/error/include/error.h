#ifndef _ERROR_H_
#define _ERROR_H_

#include <stdio.h>

//Lo iremos complejizando a medida que sea necesario
typedef struct{
    uint8_t code;
    char *message;
} error_t;

//Algunos ejemplos uwu
#define TANGUITO_OK           ((error_t){0, "OK"})
#define TANGUITO_ERROR        ((error_t){1, "Error"})
#define TANGUITO_INVALID_PARAM ((error_t){2, "Parametro invalido"})

#endif