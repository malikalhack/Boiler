/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    feedback.c
 * @version 0.1.5
 * @authors Anton Chernov
 * @date    2026-11-04
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "bsp.h"
#include "display.h"
#include "feedback.h"
/****************************** Module variables ******************************/

static bool error_active;
static AcroTick_t last_tick;
static AcroTick_t blink_phase;
static AcroTick_t beep_remaining;

/********************** Application Programming Interface *********************/

void feedback_init(bool error) {
    error_active = error;
    last_tick = bsp_get_time();
    blink_phase = 0U;
    beep_remaining = FEEDBACK_BEEP_TICKS;
    display_set_leds(FEEDBACK_LED_MASK, FEEDBACK_LED_MASK);
    bsp_buzzer_set(true);
}
/*----------------------------------------------------------------------------*/

void feedback_set_error(bool error) {
    if (error_active != error) {
        feedback_update(NULL);
        error_active = error;
        blink_phase = 0U;
        display_set_leds(FEEDBACK_LED_MASK, FEEDBACK_LED_MASK);
    }
}
/*----------------------------------------------------------------------------*/

void feedback_update(void *context) {
    AcroTick_t now;
    AcroTick_t elapsed;
    bool led_on;

    (void)context;
    now = bsp_get_time();
    elapsed = (AcroTick_t)(now - last_tick);
    last_tick = now;
    if (beep_remaining != 0U) {
        if (elapsed >= beep_remaining) {
            beep_remaining = 0U;
            bsp_buzzer_set(false);
        }
        else {
            beep_remaining -= elapsed;
        }
    }

    blink_phase = (AcroTick_t)(
        ((uint32_t)blink_phase + elapsed) % FEEDBACK_BLINK_TICKS);
    led_on = !error_active || (blink_phase < FEEDBACK_BLINK_TICKS / 2U);
    display_set_leds(FEEDBACK_LED_MASK, led_on ? FEEDBACK_LED_MASK : 0U);
}
/******************************************************************************/
