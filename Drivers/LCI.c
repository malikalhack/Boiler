/**
 * @brief Seven-segment indicator driver for use with the dynamic process manager
 * @file    LCI.c
 * @version 1.0.0
 * @authors Anton Chernov
 * @date    10.05.2019
 * @date    26.08.2022
 */
/****************************** Included files ********************************/
#include <avr/io.h>
#include "LCI.h"    /* Header file for working with seven-segment indicator */
#include "bsp.h"
/******************************** Definition **********************************/

/**************************** Private  variables ******************************/
static BYTE sign_num = 0; /* Counter for indicator */
static BYTE lci_buf[MAX_SIGN_NUM];
/******************************** Entry point *********************************/
/** @fn display */
void lci_display (void *pntr) {
    register BYTE temp_sign_num = sign_num;
    register BYTE segment = *(lci_buf + temp_sign_num);
    bsp_com_lci_off();
    bsp_send2port(segment);
    bsp_com_lci_on(temp_sign_num++);
    temp_sign_num %= MAX_SIGN_NUM;
    sign_num = temp_sign_num;
}
/************************************* API ************************************/
/** @fn init_display */
void lci_init (void) {
    for (register BYTE i = 0; i < MAX_SIGN_NUM; i++) {
        *(lci_buf + i) = 0xFF;
    }
}
/******************************************************************************/
/** @fn set_display */
void lci_set (BYTE * p_array) {
    for (register BYTE i = 0; i < MAX_SIGN_NUM - 1; i++) {
        *(lci_buf + i) = *(p_array + i);
    }
}
/******************************************************************************/
/** @fn set_led_buf */
void lci_set_led (register BYTE mask) {
    lci_buf [MAX_SIGN_NUM - 1] = mask;
}
/******************************************************************************/
