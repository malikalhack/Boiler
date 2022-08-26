/**
 * @brief The implementation of "run to completion" state machine.
 * @file    StateMachine.h
 * @version 1.0.0
 * @authors Anton Chernov
 * @date    16.06.2019
 * @date    17.11.2020
 * @note    MCU:   ATmega16
 * @note    Clock: 14.7456 MHz
 */
/****************************** Included files ********************************/
#include "StateMachine.h" /* State Machine private header file */
/******************************** Definition **********************************/
enum SM_ERRORS {
	SUCCESS = 0,
	NO_TRANSFER, /* 1 */
	INIT_ERROR   /* 2 */
};
/**************************** Private prototypes ******************************/

/********************* Private State machine prototypes ***********************/

/**************************** Private  variables ******************************/

/************************** Additional Definition *****************************/

/************************************ API *************************************/
void sm_start(WORD * const ptr_time) {  
    tm_init(ptr_time);          /* Reset and initialize the dispatcher */

}
/************************* State machine functions ****************************/
