/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    buttons.c
 * @version 0.1.7
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "buttons.h"
#include "bsp.h"
/********************************* Definitions ********************************/

typedef struct SButtonState {
    AcroTick_t debounce;
    AcroTick_t repeat;
} SButtonState_t;

/****************************** Module variables ******************************/

static const EButton_t button_bits[] = {
    eButtonDown, eButtonRight, eButtonUp, eButtonLeft, eButtonEnter
};
static SButtonState_t states[sizeof(button_bits) / sizeof(button_bits[0])];
static uint8_t candidate;
static uint8_t pressed;
static AcroTick_t last_tick;
static ButtonHandler_t event_handler;

/***************************** Private prototypes *****************************/

static void report_event(EButton_t button, EButtonEvent_t event);
static void poll_button(uint8_t index, uint8_t raw, AcroTick_t elapsed);

/****************************** Private functions *****************************/

static void report_event(EButton_t button, EButtonEvent_t event) {
    if (event_handler != NULL) {
        event_handler(button, event);
    }
}
/*----------------------------------------------------------------------------*/

static void poll_button(uint8_t index, uint8_t raw, AcroTick_t elapsed) {
    uint8_t bit = (uint8_t)button_bits[index];
    SButtonState_t *state = &states[index];

    if (((raw ^ candidate) & bit) != 0U) {
        candidate ^= bit;
        state->debounce = BUTTON_DEBOUNCE_TICKS;
    }
    else if (((candidate ^ pressed) & bit) != 0U) {
        if (elapsed < state->debounce) {
            state->debounce -= elapsed;
        }
        else {
            pressed ^= bit;
            state->repeat = BUTTON_HOLD_TICKS;
            report_event(button_bits[index], (pressed & bit) != 0U ?
                         eButtonPressed : eButtonReleased);
        }
    }
    else if ((raw & pressed & bit & (eButtonUp | eButtonDown)) != 0U) {
        if (elapsed < state->repeat) {
            state->repeat -= elapsed;
        }
        else {
            state->repeat = BUTTON_REPEAT_TICKS -
                (elapsed - state->repeat) % BUTTON_REPEAT_TICKS;
            report_event(button_bits[index], eButtonRepeat);
        }
    }
}

/********************** Application Programming Interface *********************/
void buttons_init(ButtonHandler_t handler) {
    uint8_t index;

    event_handler = handler;
    candidate = bsp_get_buttons();
    pressed = 0U;
    last_tick = bsp_get_time();
    for (index = 0U; index < sizeof(states) / sizeof(states[0]); index++) {
        states[index].debounce = BUTTON_DEBOUNCE_TICKS;
        states[index].repeat = BUTTON_HOLD_TICKS;
    }
}
/*----------------------------------------------------------------------------*/

void buttons_poll(void *context) {
    AcroTick_t now;
    AcroTick_t elapsed;
    uint8_t raw;
    uint8_t index;

    (void)context;
    now = bsp_get_time();
    elapsed = (AcroTick_t)(now - last_tick);
    last_tick = now;
    raw = bsp_get_buttons();
    for (index = 0U; index < sizeof(states) / sizeof(states[0]); index++) {
        poll_button(index, raw, elapsed);
    }
}
/*----------------------------------------------------------------------------*/

uint8_t buttons_get_state(void) {
    return pressed;
}
/******************************************************************************/
