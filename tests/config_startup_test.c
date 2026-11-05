/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    config_startup_test.c
 * @version 0.1.5
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

/**
 * @def main
 * @brief Rename firmware entry point for fake configuration startup tests.
 */
#define main firmware_main
#include "../src/main.c"
#undef main

/****************************** Module variables ******************************/

static volatile AcroTick_t tick;
static bool interrupts_enabled;
static bool buzzer;
static uint8_t segments;
static uint8_t selected;
static uint8_t registered;
static jmp_buf scheduler_exit;

/********************** Application Programming Interface *********************/

void bspInit(void) {
    interrupts_enabled = false;
}
/*----------------------------------------------------------------------------*/

void bspStart(void) {
    interrupts_enabled = true;
}
/*----------------------------------------------------------------------------*/

volatile AcroTick_t *bspGetTick(void) {
    return &tick;
}
/*----------------------------------------------------------------------------*/

AcroTick_t bsp_get_time(void) {
    return tick;
}
/*----------------------------------------------------------------------------*/

uint8_t bsp_get_buttons(void) {
    return 0U;
}
/*----------------------------------------------------------------------------*/

void bsp_buzzer_set(bool enabled) {
    buzzer = enabled;
}
/*----------------------------------------------------------------------------*/

void bsp_display_write(uint8_t data, uint8_t mask) {
    segments = data;
    selected = mask;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroInit(volatile AcroTick_t const *source) {
    assert(source == &tick);
    assert(!interrupts_enabled);
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroAddTask(
    uint8_t descriptor, AcroTask_t *task, AcroParam_t params,
    uint8_t mode, AcroTick_t period, uint8_t priority
) {
    assert(!interrupts_enabled);
    assert(params == NULL);
    assert(mode == ePeriodic);
    assert(priority == 0U);
    assert(descriptor >= eTaskDisplay && descriptor <= eTaskSensors);
    assert((registered & (1U << descriptor)) == 0U);
    registered |= 1U << descriptor;
    if (descriptor == eTaskSensors) {
        assert(task == check_sensors);
        assert(period == BOILER_TICK_HZ / 10U);
    }
    else {
        assert(period == 1U);
    }
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroRun(void) {
    assert(interrupts_enabled);
    assert(buzzer);
    assert(registered == ((1U << eTaskDisplay) | (1U << eTaskFeedback) |
                          (1U << eTaskButtons) | (1U << eTaskSensors)));
    longjmp(scheduler_exit, 1);
}
/*----------------------------------------------------------------------------*/

/****************************** Private functions *****************************/

static void check_frame(EError_t error, bool led_on) {
    uint8_t position;
    uint8_t expected;

    for (position = 0U; position < DISPLAY_SLOTS; position++) {
        display_scan(NULL);
        assert(selected == (1U << position));
        expected = eGlyphDash;
        if (position == DISPLAY_DIGITS) {
            expected = led_on ? FEEDBACK_LED_MASK : 0U;
        }
        else if (error != eErrorNone) {
            expected = position == 0U ? eGlyphE : eGlyph0;
            if (position == DISPLAY_DIGITS - 1U) {
                expected = error == eErrorSensor1 ? eGlyph2 :
                           error == eErrorSensor2 ? eGlyph3 : eGlyph4;
            }
        }
        assert(segments == expected);
    }
}
/*----------------------------------------------------------------------------*/

int main(void) {
    EError_t expected;

    if (setjmp(scheduler_exit) == 0) {
        firmware_main();
        assert(false);
    }
    expected = (SENSOR_FAKE_PRESENT_MASK & 1U) == 0U ? eErrorSensor1 :
        CONFIG_TEST_PROFILE == eProfileSingle ?
        ((SENSOR_FAKE_PRESENT_MASK & 2U) != 0U ?
         eErrorUnknownSensor : eErrorNone) :
        ((SENSOR_FAKE_PRESENT_MASK & 2U) == 0U ? eErrorSensor2 : eErrorNone);
    assert(active_error == expected);
    check_frame(expected, true);
    tick = FEEDBACK_BLINK_TICKS / 2U;
    feedback_update(NULL);
    assert(!buzzer);
    check_frame(expected, expected == eErrorNone);
    assert(sensors_fake_set_present(0U, true) == eResultOk);
    assert(sensors_fake_set_present(
        1U, CONFIG_TEST_PROFILE == eProfileTwo) == eResultOk);
    check_sensors(NULL);
    assert(active_error == eErrorNone);
    check_frame(eErrorNone, true);
    assert(!buzzer);
    assert(sensors_fake_set_present(0U, false) == eResultOk);
    check_sensors(NULL);
    assert(active_error == eErrorSensor1);
    check_frame(eErrorSensor1, true);
    assert(!buzzer);
    puts("RAM configuration startup tests passed");
    return 0;
}
/******************************************************************************/
