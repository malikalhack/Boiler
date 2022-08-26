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
#include "bsp.h"
/******************************** Definition **********************************/
enum descriptors {
    STATE_MACHINE = 1,
    DISPLAY,
    CHANNELS,
    BUZZER,
    KEYBOARD,
    SENSORS,
    HEATER,
    WATCHDOG,
    MAX_DESCRIPTOR
};

enum CURRENT_STATE {
    ERROR = 0,
    INIT_STAGE_1,
    INIT_STAGE_2,
    MAN_SENS_INIT,
    INIT_STAGE_3,
    IDLE,
    WORK
};

enum SM_ERRORS {
    SUCCESS = 0,
    NO_TRANSFER, /* 1 */
    INIT_ERROR   /* 2 */
};

/************************** Set values of periods *****************************/
/*  All values are in milliseconds  */
#define CHANGE_PERIOD       3000
#define HEAT_CONTROL_PERIOD 2000
#define WDT_RST_PERIOD      1500
#define SENS_READING_PERIOD 1000
#define SQUEAK_DURATION     200
#define SCAN_KEY_PERIOD     SCAN_SPEC_PERIOD
#define DISP_UPD_FREQ       5
#define UNDEFINED           0
/*********************************** Limits ***********************************/
#define TOP_LIMIT           90
#define BOT_LIMIT           25
#define HYST_LIMIT          15
#define CLEARANCE           5

/**************************** Private prototypes ******************************/

/********************* Private State machine prototypes ***********************/

/**************************** Private  variables ******************************/

/************************** Additional Definition *****************************/

/************************************ API *************************************/
void sm_start(WORD * const ptr_time) {  
    bsp_gpio_init();            /* Setting the GPIO in a safe condition */
    tm_init(ptr_time);          /* Reset and initialize the dispatcher */
    bsp_systimer_start();
}
/************************* State machine functions ****************************/
