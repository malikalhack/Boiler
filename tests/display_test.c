/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    display_test.c
 * @version 0.1.12
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "sensors.h"
#include "feedback.h"
#include "bsp.h"

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
static volatile AcroTick_t tick;
static bool buzzer_on;

/********************** Application Programming Interface *********************/

void bsp_display_write(uint8_t segments, uint8_t select_mask) {
    last_segments = segments;
    last_select = select_mask;
    write_count++;
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

static void check_temperature(int16_t raw, const char *text) {
    static const uint8_t glyphs[] = {
        eGlyph0, eGlyph1, eGlyph2, eGlyph3, eGlyph4,
        eGlyph5, eGlyph6, eGlyph7, eGlyph8, eGlyph9
    };
    SDisplayFrame_t expected = {{0U, 0U, 0U, 0U, 0xA5U}};
    uint8_t position = DISPLAY_DIGITS - strlen(text) +
                       (strchr(text, '.') != NULL ? 1U : 0U);

    while (*text != '\0') {
        if (*text == '.') {
            assert(position != 0U);
            expected.slots[position - 1U] |= eGlyphPoint;
        }
        else {
            assert(position < DISPLAY_DIGITS);
            expected.slots[position++] = *text == '-' ?
                eGlyphDash : glyphs[(uint8_t)(*text - '0')];
        }
        text++;
    }
    assert(position == DISPLAY_DIGITS);
    assert(display_show_temperature(raw) == eResultOk);
    check_frame(expected);
}
/*----------------------------------------------------------------------------*/

static void test_temperatures(void) {
    SDisplayFrame_t original = {{1U, 2U, 3U, 4U, 0xA5U}};
    int16_t raw;
    int rounded;
    char text[16];
    double degrees;
    uint8_t position;

    display_init();
    display_set_frame(original);
    check_temperature(-55 * SENSOR_TEMP_SCALE, "-55.0");
    check_temperature(-168, "-10.5");
    check_temperature(-164, "-10.3");
    check_temperature(-160, "-10.0");
    check_temperature(-159, "-9.9");
    check_temperature(-8, "-0.5");
    check_temperature(-4, "-0.3");
    check_temperature(-1, "-0.1");
    check_temperature(0, "0.0");
    check_temperature(1, "0.1");
    check_temperature(25 * SENSOR_TEMP_SCALE, "25.0");
    check_temperature(1599, "99.9");
    check_temperature(4, "0.3");
    check_temperature(1600, "100.0");
    check_temperature(1608, "100.5");
    check_temperature(125 * SENSOR_TEMP_SCALE, "125.0");
    for (raw = -55 * SENSOR_TEMP_SCALE;
         raw <= 125 * SENSOR_TEMP_SCALE; raw++) {
        degrees = (double)raw / SENSOR_TEMP_SCALE;
        rounded = (int)(degrees * 10.0 + (raw < 0 ? -0.5 : 0.5));
        snprintf(text, sizeof(text), "%.1f", rounded / 10.0);
        check_temperature(raw, text);
    }
    display_set_frame(original);
    assert(display_show_temperature(-55 * SENSOR_TEMP_SCALE - 1) ==
           eResultOutOfRange);
    assert(display_show_temperature(125 * SENSOR_TEMP_SCALE + 1) ==
           eResultOutOfRange);
    assert(display_show_temperature(INT16_MIN) == eResultOutOfRange);
    assert(display_show_temperature(INT16_MAX) == eResultOutOfRange);
    check_frame(original);
    check_slot(0U, original.slots[0]);
    assert(display_show_temperature(0) == eResultOk);
    for (position = 1U; position < DISPLAY_SLOTS; position++) {
        check_slot(position, original.slots[position]);
    }
    check_frame((SDisplayFrame_t){{eGlyphBlank, eGlyphBlank,
                                 eGlyph0 | eGlyphPoint, eGlyph0, 0xA5U}});
}
/*----------------------------------------------------------------------------*/

static void test_feedback(void) {
    static const EDisplayLed_t modes[] = {
        eLedMode1, eLedMode2, eLedMode3, eLedMode4
    };
    static const EDisplayLed_t invalid_modes[] = {
        eLedHeating, eLedAtSetpoint, eLedChannel1, eLedChannel2,
        (EDisplayLed_t)0U, (EDisplayLed_t)(eLedMode1 | eLedMode2)
    };
    SDisplayFrame_t frame = {{eGlyph1, eGlyph2, eGlyph3, eGlyph4, 0x39U}};
    SDisplayFrame_t expected;
    uint16_t elapsed;
    uint8_t index;

    assert(eLedHeating == 0x01U && eLedAtSetpoint == 0x20U);
    assert(eLedChannel1 == 0x10U && eLedChannel2 == 0x08U);
    assert(eLedMode1 == 0x02U && eLedMode2 == 0x40U);
    assert(eLedMode3 == 0x04U && eLedMode4 == 0x80U);
    display_init();
    display_set_frame(frame);
    display_set_leds(DISPLAY_MODE_LED_MASK, DISPLAY_MODE_LED_MASK);
    expected = frame;
    expected.slots[DISPLAY_DIGITS] |= eLedMode3;
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
        if (elapsed % FEEDBACK_BLINK_TICKS < FEEDBACK_BLINK_TICKS / 2U) {
            expected.slots[DISPLAY_DIGITS] |= eLedMode3;
        }
        check_frame(expected);
        assert(!buzzer_on);
    }

    feedback_set_error(false);
    expected.slots[DISPLAY_DIGITS] |= eLedMode3;
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

    feedback_set_error(false);
    for (index = 0U; index < sizeof(modes) / sizeof(modes[0]); index++) {
        assert(feedback_set_mode(modes[index]) == eResultOk);
        expected.slots[DISPLAY_DIGITS] =
            frame.slots[DISPLAY_DIGITS] | modes[index];
        check_frame(expected);
        feedback_set_error(true);
        tick += FEEDBACK_BLINK_TICKS / 2U - 1U;
        feedback_update(NULL);
        check_frame(expected);
        tick++;
        feedback_update(NULL);
        check_frame(frame);
        assert(feedback_set_mode(modes[index]) == eResultOk);
        feedback_set_error(true);
        check_frame(frame);
        tick += FEEDBACK_BLINK_TICKS / 2U;
        feedback_update(NULL);
        check_frame(expected);
        feedback_set_error(false);
        assert(!buzzer_on);
    }
    for (index = 0U;
         index < sizeof(invalid_modes) / sizeof(invalid_modes[0]); index++) {
        assert(feedback_set_mode(invalid_modes[index]) ==
               eResultInvalidArgument);
        check_frame(expected);
        assert(!buzzer_on);
    }
    feedback_set_error(true);
    tick += FEEDBACK_BLINK_TICKS / 2U;
    feedback_update(NULL);
    check_frame(frame);
    assert(feedback_set_mode(eLedMode3) == eResultOk);
    expected.slots[DISPLAY_DIGITS] = frame.slots[DISPLAY_DIGITS] | eLedMode3;
    check_frame(expected);
    assert(!buzzer_on);
}
/*----------------------------------------------------------------------------*/

int main(void) {
    test_frames();
    test_errors();
    test_temperatures();
    test_feedback();
    puts("Display tests passed");
    return 0;
}
/******************************************************************************/
