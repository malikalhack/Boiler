/**
 * @brief The implementation of "run to completion" state machine.
 * @file    StateMachine.h
 * @version 1.0.0
 * @authors Anton Chernov
 * @date    16.06.2019
 * @date    26.08.2022
 * @note    MCU:   ATmega16
 * @note    Clock: 14.7456 MHz
 */
/****************************** Included files ********************************/
#include "StateMachine.h" /* State Machine private header file */
#include "bsp.h"
#include "WDT.h"
#include "LCI.h"

/******************************** Definition **********************************/
enum descriptors_t {
    FORBIDDEN_DESCRIPTOR = 0,
    STATE_MACHINE,
    DISPLAY,
    CHANNELS,
    BUZZER,
    KEYBOARD,
    SENSORS,
    HEATER,
    WATCHDOG,
    MAX_DESCRIPTOR
};

enum sm_errors_t {
    SUCCESS = 0,
    NO_TRANSFER, /* 1 */
    INIT_ERROR   /* 2 */
};

/********************************** States ************************************/
enum boiler_state_t {
    STATE_ERROR = 0,
    STATE_INIT_1,
    STATE_INIT_2,
    STATE_MANUAL_SENSOR_INIT,
    STATE_INIT_3,
    STATE_IDLE,
    STATE_WORK,
    MAX_BOILER_STATE
};

enum display_state_t {
    SHOW_DASHES = MAX_BOILER_STATE,
    SHOW_FIRST_TEMP,
    SHOW_SECOND_TEMP,
    SHOW_ON,
    SHOW_OFF,
    SHOW_SET,
    SHOW_MODE,
    SHOW_HEATING_MODE,
    SHOW_UPPER_LIM,
    SHOW_LOWER_LIM,
    SHOW_HYSTERESIS,
    SHOW_QUIT,
    SHOW_MODE_SEL,
    SHOW_HEAT_MODE_SEL,
    SHOW_UPPER_LIM_SEL,
    SHOW_LOWER_LIM_SEL,
    SHOW_HYST_SEL,
    SHOW_ERROR,
    MAX_DISPLAY_STATE
};

/************************** Set values of periods *****************************/
/*  All values are in milliseconds  */
#define CHANGE_PERIOD       (3000)
#define HEAT_CONTROL_PERIOD (2000)
#define WDT_RST_PERIOD      (1500)
#define SENS_READING_PERIOD (1000)
#define SQUEAK_DURATION     (200)
//#define SCAN_KEY_PERIOD     SCAN_SPEC_PERIOD
#define DISP_UPD_FREQ       (5)
#define UNDEFINED           (0)
/*********************************** Limits ***********************************/
#define TOP_LIMIT           (90)
#define BOT_LIMIT           (25)
#define HYST_LIMIT          (15)
#define CLEARANCE           (5)
/*********************************** LEDs *************************************/
#define MASK_HEAT           BIT(0)
#define MASK_ON_MODE        BIT(5)
#define MASK_HEATER_1       BIT(4)
#define MASK_HEATER_2       BIT(3)
#define MASK_MODE_1         BIT(1)
#define MASK_MODE_2         BIT(6)
#define MASK_MODE_3         BIT(2)
#define MASK_MODE_4         BIT(7)
/*********************************** KEYs *************************************/
//#define KEY_UP              BIT(key_3)
//#define KEY_DOWN            BIT(key_1)
//#define KEY_LEFT            BIT(key_4)
//#define KEY_RIGHT           BIT(key_2)
//#define KEY_ENTER           BIT(key_5)
/**************************** Private prototypes ******************************/

/********************* Private State machine prototypes ***********************/
void state_machine (void*);
//void switch_temp_channel (void * ptr);
/**************************** Private  variables ******************************/
static BYTE current_state = STATE_INIT_1;
static BYTE lcd_buf[MAX_SIGN_NUM - 1] = { f_dash, f_dash, f_dash, f_dash };
//static BYTE font[10] = {
//    f_zero, f_one, f_two, f_three, f_four,
//    f_five, f_six, f_seven, f_eight, f_nine
//};
/************************** Additional Definition *****************************/

/************************************ API *************************************/
void sm_start(WORD * const ptr_time) {  
    bsp_gpio_init();            /* Setting the GPIO in a safe condition */
    tm_init(ptr_time);          /* Reset and initialize the dispatcher */
    lci_init();
    wdt_init(WD_2048K_CYCLES);
    bsp_systimer_start();

    tm_add_task(
        STATE_MACHINE,
        &state_machine,
        &current_state,
        PERIODIC_MODE,
        500
    );
}
/************************* State machine functions ****************************/
void state_machine (void * ptr) {
    BYTE * state = (BYTE *) ptr;
    switch (*state) {
/*----------------------------------------------------------------------------*/
        case STATE_INIT_1: {
            wdt_start();

            tm_add_task(
                DISPLAY,
                &lci_display,
                NULLPTR,
                PERIODIC_MODE,
                DISP_UPD_FREQ
            );
            tm_add_task(
                WATCHDOG,
                &wd_throw_bone,
                NULLPTR,
                PERIODIC_MODE,
                WDT_RST_PERIOD
            );

            *state = STATE_INIT_2;
            break; /* Release process */
        }
/*----------------------------------------------------------------------------*/
        case STATE_INIT_2: {
        lci_set(lcd_buf);
        lci_set_led(0b00000000); //TODO: Fix this
            *state = STATE_IDLE;
            break;
        }
/*----------------------------------------------------------------------------*/
        case STATE_IDLE:
            break;
/*----------------------------------------------------------------------------*/
        case STATE_ERROR:
            tm_kill_task(WATCHDOG);
            tm_kill_task(CHANNELS);
            break;
/*----------------------------------------------------------------------------*/
        default:
            *state = STATE_ERROR;
/*----------------------------------------------------------------------------*/
    }
}
/******************************************************************************/

