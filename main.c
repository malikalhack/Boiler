/**
 * @brief Program Entry point
 * @note ATmega16 - 14.7456 MHz
 * @file    main.c
 * @version 1.0.0
 * @authors Anton Chernov
 * @date    16.04.2019
 */

/******************************** Included files ******************************/
#include "main.h"
/****************************** Global variables ******************************/
static unsigned int SysTime;	/* Time counter */
/******************************** Entry point *********************************/
int main(void) {
    SysTime = 0;		/* Time counter initialize */
/******************************* Endlessly loop *******************************/
    while (1);
/******************************************************************************/
}

