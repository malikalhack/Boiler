/**
 * @file    display_test.c
 * @version 0.1.0
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>

#include "display.h"
#include "storage.h"
#include "bsp.h"

/**
 * @def main
 * @brief Rename the firmware entry point for startup tests.
 */
#define main firmware_main
#include "../src/main.c"
#undef main

/********************************* Definitions ********************************/

/**
 * @def TEST_BYTE_VALUES
 * @brief Number of possible segment or LED byte patterns.
 */
#define TEST_BYTE_VALUES (256U)

/****************************** Module variables ******************************/

static uint8_t last_segments;
static uint8_t last_select;
static unsigned int write_count;
static bool storage_valid;
static bool ports_initialized;
static bool interrupts_enabled;
static volatile AcroTick_t tick;
static AcroTask_t *registered_task;
static AcroTask_t *feedback_task;
static bool buzzer_on;
static jmp_buf scheduler_exit;

/********************** Application Programming Interface *********************/

void bsp_display_write(uint8_t segments, uint8_t select_mask) {
    last_segments = segments;
    last_select = select_mask;
    write_count++;
}
/*----------------------------------------------------------------------------*/

void bspInit(void) {
    ports_initialized = true;
    interrupts_enabled = false;
}
/*----------------------------------------------------------------------------*/

volatile AcroTick_t *bspGetTick(void) {
    return &tick;
}
/*----------------------------------------------------------------------------*/

void bspStart(void) {
    interrupts_enabled = true;
}
/*----------------------------------------------------------------------------*/

AcroTick_t bsp_get_time(void) {
    return tick;
}
/*----------------------------------------------------------------------------*/

void bsp_buzzer_set(bool enabled) {
    buzzer_on = enabled;
}
/*----------------------------------------------------------------------------*/

EResult_t storageInit(void) {
    assert(ports_initialized);
    assert(!interrupts_enabled);
    return storage_valid ? eResultOk : eResultNoConfig;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroInit(volatile AcroTick_t const *time_source) {
    assert(time_source == &tick);
    assert(!interrupts_enabled);
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroAddTask(
    uint8_t descriptor,
    AcroTask_t *task,
    AcroParam_t params,
    uint8_t mode,
    AcroTick_t period,
    uint8_t priority
) {
    if (descriptor == eTaskDisplay) {
        assert(task == display_scan);
        assert(priority == TASK_PRIORITY_DISPLAY);
        registered_task = task;
    }
    else {
        assert(descriptor == eTaskFeedback);
        assert(task == feedback_update);
        assert(priority == TASK_PRIORITY_FEEDBACK);
        feedback_task = task;
    }
    assert(params == NULL);
    assert(mode == ePeriodic);
    assert(period == 1U);
    assert(!interrupts_enabled);
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroRun(void) {
    assert(interrupts_enabled);
    assert(registered_task != NULL);
    assert(feedback_task != NULL);
    assert(buzzer_on);
    longjmp(scheduler_exit, 1);
}

/****************************** Private functions *****************************/
static void check_slot(uint8_t position, uint8_t segments) {
    unsigned int previous_count;

    previous_count = write_count;
    display_scan(NULL);
    assert(write_count == previous_count + 1U);
    assert(last_select == (uint8_t)(1U << position));
    assert(last_segments == segments);
}
/*----------------------------------------------------------------------------*/

static void check_frame(SDisplayFrame_t frame) {
    uint8_t position;

    for (position = 0U; position < DISPLAY_SLOTS; position++) {
        check_slot(position, frame.slots[position]);
    }
}
/*----------------------------------------------------------------------------*/

static void test_frames(void) {
    SDisplayFrame_t first = {{0x01U, 0x02U, 0x04U, 0x08U, 0x10U}};
    SDisplayFrame_t next = {{0x20U, 0x40U, 0x80U, 0xFFU, 0x55U}};
    SDisplayFrame_t dashes = {
        {eGlyphDash, eGlyphDash, eGlyphDash, eGlyphDash, 0U}
    };
    uint8_t position;
    uint8_t boundary;
    uint16_t pattern;

    display_init();
    assert(last_segments == 0U);
    assert(last_select == 0U);
    check_frame(dashes);

    for (boundary = 1U; boundary < DISPLAY_SLOTS; boundary++) {
        display_set_frame(first);
        for (position = 0U; position < boundary; position++) {
            check_slot(position, first.slots[position]);
        }
        display_set_frame(dashes);
        display_set_frame(next);
        for (position = boundary; position < DISPLAY_SLOTS;
             position++) {
            check_slot(position, first.slots[position]);
        }
        check_frame(next);
        check_frame(next);
    }

    for (pattern = 0U; pattern < TEST_BYTE_VALUES; pattern++) {
        for (position = 0U; position < DISPLAY_SLOTS; position++) {
            first.slots[position] = (uint8_t)pattern;
        }
        display_set_frame(first);
        check_frame(first);
    }

    display_scan(NULL);
    display_init();
    check_frame(dashes);
}
/*----------------------------------------------------------------------------*/

static void test_startup(bool valid, SDisplayFrame_t expected) {
    storage_valid = valid;
    registered_task = NULL;
    feedback_task = NULL;
    tick = 0U;
    if (setjmp(scheduler_exit) == 0) {
        (void)firmware_main();
        assert(false);
    }
    check_frame(expected);
    tick = FEEDBACK_BEEP_TICKS;
    feedback_task(NULL);
    assert(!buzzer_on);
}
/*----------------------------------------------------------------------------*/

static void test_errors(void) {
    const EError_t errors[] = {
        eErrorConfigInvalid, eErrorSensor1, eErrorSensor2,
        eErrorUnknownSensor, eErrorCrc, eErrorDeviceCount,
        eErrorDs2480b, eErrorSoft1Wire, eErrorWatchdog
    };
    const uint8_t expected_glyphs[] = {
        eGlyph1, eGlyph2, eGlyph3, eGlyph4, eGlyph5,
        eGlyph6, eGlyph7, eGlyph8, eGlyph9
    };
    SDisplayFrame_t original = {{0x01U, 0x02U, 0x04U, 0x08U, 0xA5U}};
    SDisplayFrame_t expected = {
        {eGlyphE, eGlyph0, eGlyph0, eGlyph0, 0xA5U}
    };
    uint8_t index;
    uint8_t position;

    display_init();
    display_set_frame(original);
    for (index = 0U; index < sizeof(errors) / sizeof(errors[0]); index++) {
        assert((unsigned int)errors[index] == index + 1U);
        assert(display_show_error(errors[index]) == eResultOk);
        expected.slots[DISPLAY_DIGITS - 1U] = expected_glyphs[index];
        check_frame(expected);
    }

    display_set_frame(original);
    assert(display_show_error(eErrorNone) == eResultInvalidArgument);
    assert(display_show_error((EError_t)-1) == eResultInvalidArgument);
    assert(display_show_error((EError_t)TEST_BYTE_VALUES) ==
           eResultInvalidArgument);
    check_frame(original);

    check_slot(0U, original.slots[0]);
    assert(display_show_error(eErrorConfigInvalid) == eResultOk);
    for (position = 1U; position < DISPLAY_SLOTS; position++) {
        check_slot(position, original.slots[position]);
    }
    expected.slots[DISPLAY_DIGITS - 1U] = eGlyph1;
    check_frame(expected);
}
/*----------------------------------------------------------------------------*/

static void test_feedback(void) {
    SDisplayFrame_t frame = {{eGlyph1, eGlyph2, eGlyph3, eGlyph4, 0xA1U}};
    SDisplayFrame_t expected;
    uint16_t elapsed;

    display_init();
    display_set_frame(frame);
    expected = frame;
    expected.slots[DISPLAY_DIGITS] |= FEEDBACK_LED_MASK;
    tick = 0U;
    feedback_init(false);
    for (elapsed = 0U; elapsed <= FEEDBACK_BLINK_TICKS; elapsed++) {
        tick = elapsed;
        feedback_update(NULL);
        assert(buzzer_on == (elapsed < FEEDBACK_BEEP_TICKS));
        check_frame(expected);
    }

    feedback_set_error(true);
    for (elapsed = 0U; elapsed <= 2U * FEEDBACK_BLINK_TICKS; elapsed++) {
        tick = FEEDBACK_BLINK_TICKS + elapsed;
        feedback_update(NULL);
        expected.slots[DISPLAY_DIGITS] = frame.slots[DISPLAY_DIGITS];
        if ((elapsed % FEEDBACK_BLINK_TICKS) <
            FEEDBACK_BLINK_TICKS / 2U) {
            expected.slots[DISPLAY_DIGITS] |= FEEDBACK_LED_MASK;
        }
        check_frame(expected);
        assert(!buzzer_on);
    }

    feedback_set_error(false);
    expected.slots[DISPLAY_DIGITS] |= FEEDBACK_LED_MASK;
    check_frame(expected);
    tick += FEEDBACK_BLINK_TICKS;
    feedback_update(NULL);
    check_frame(expected);
    assert(!buzzer_on);

    tick = (AcroTick_t)-1;
    tick -= FEEDBACK_BEEP_TICKS / 2U;
    feedback_init(true);
    tick += FEEDBACK_BEEP_TICKS - 1U;
    feedback_update(NULL);
    assert(buzzer_on);
    tick++;
    feedback_update(NULL);
    assert(!buzzer_on);
    tick += FEEDBACK_BLINK_TICKS / 2U - FEEDBACK_BEEP_TICKS;
    feedback_update(NULL);
    check_frame(frame);
    tick += FEEDBACK_BLINK_TICKS;
    feedback_set_error(true);
    feedback_update(NULL);
    check_frame(frame);
    tick += FEEDBACK_BLINK_TICKS / 2U;
    feedback_update(NULL);
    check_frame(expected);
}
/*----------------------------------------------------------------------------*/

int main(void) {
    SDisplayFrame_t error = {
        {eGlyphE, eGlyph0, eGlyph0, eGlyph1, FEEDBACK_LED_MASK}
    };
    SDisplayFrame_t dashes = {
        {eGlyphDash, eGlyphDash, eGlyphDash, eGlyphDash, FEEDBACK_LED_MASK}
    };

    test_frames();
    test_errors();
    test_feedback();
    test_startup(false, error);
    test_startup(true, dashes);
    puts("Display tests passed");
    return 0;
}
/******************************************************************************/
