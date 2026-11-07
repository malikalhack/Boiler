/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    display.c
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "display.h"
#include "bsp.h"
#include "sensors.h"

/********************************* Definitions ********************************/
/**
 * @def DISPLAY_DECIMAL_BASE
 * @brief Radix used to format numeric values.
 */
#define DISPLAY_DECIMAL_BASE (10U)

/****************************** Module variables ******************************/

static const uint8_t digit_glyphs[DISPLAY_DECIMAL_BASE] = {
    eGlyph0, eGlyph1, eGlyph2, eGlyph3, eGlyph4,
    eGlyph5, eGlyph6, eGlyph7, eGlyph8, eGlyph9
};
static SDisplayFrame_t pending_frame;
static SDisplayFrame_t active_frame;
static uint8_t scan_position;

/********************** Application Programming Interface *********************/

void display_init(void) {
    bsp_display_write(0U, 0U);
    display_show_dashes();
    pending_frame.slots[DISPLAY_DIGITS] = 0U;
    active_frame = pending_frame;
    scan_position = 0U;
}

/*----------------------------------------------------------------------------*/

void display_set_frame(SDisplayFrame_t frame) {
    pending_frame = frame;
}

/*----------------------------------------------------------------------------*/

void display_show_dashes(void) {
    uint8_t digit;

    for (digit = 0U; digit < DISPLAY_DIGITS; digit++) {
        pending_frame.slots[digit] = eGlyphDash;
    }
}
/*----------------------------------------------------------------------------*/

void display_set_digits(SDisplayFrame_t frame) {
    uint8_t digit;

    for (digit = 0U; digit < DISPLAY_DIGITS; digit++) {
        pending_frame.slots[digit] = frame.slots[digit];
    }
}
/*----------------------------------------------------------------------------*/

void display_set_points(uint8_t mask) {
    uint8_t digit;

    for (digit = 0U; digit < DISPLAY_DIGITS; digit++) {
        pending_frame.slots[digit] =
            (pending_frame.slots[digit] & (uint8_t)~eGlyphPoint) |
            ((mask & (1U << digit)) != 0U ? eGlyphPoint : 0U);
    }
}
/*----------------------------------------------------------------------------*/

void display_set_leds(uint8_t mask, uint8_t value) {
    pending_frame.slots[DISPLAY_DIGITS] =
        (pending_frame.slots[DISPLAY_DIGITS] & (uint8_t)~mask) |
        (value & mask);
}
/*----------------------------------------------------------------------------*/

EResult_t display_show_error(EError_t error) {
    EResult_t ret_val;
    uint16_t code;
    uint8_t position;
    SDisplayFrame_t frame;

    ret_val = eResultInvalidArgument;
    if (error >= eErrorConfigInvalid && error <= eErrorWatchdog) {
        frame = pending_frame;
        frame.slots[0] = eGlyphE;
        code = (uint16_t)error;
        for (position = DISPLAY_DIGITS - 1U; position > 0U; position--) {
            frame.slots[position] = digit_glyphs[code % DISPLAY_DECIMAL_BASE];
            code /= DISPLAY_DECIMAL_BASE;
        }
        display_set_frame(frame);
        ret_val = eResultOk;
    }

    return ret_val;
}

/*----------------------------------------------------------------------------*/

EResult_t display_show_temperature(int16_t temperature) {
    SDisplayFrame_t frame = {{0U}};
    uint16_t magnitude;
    uint16_t value;
    uint8_t position = DISPLAY_DIGITS;
    bool negative = temperature < 0;
    bool fractional;

    if (
        temperature < -55 * SENSOR_TEMP_SCALE ||
        temperature > 125 * SENSOR_TEMP_SCALE
    ) {
        return eResultOutOfRange;
    }
    magnitude = negative ? (uint16_t)-temperature : (uint16_t)temperature;
    value = (magnitude * DISPLAY_DECIMAL_BASE + SENSOR_TEMP_SCALE / 2U) /
            SENSOR_TEMP_SCALE;
    fractional = value <= (negative ? 99U : 999U);
    if (!fractional) {
        value = (magnitude + SENSOR_TEMP_SCALE / 2U) / SENSOR_TEMP_SCALE;
    }
    if (fractional) {
        frame.slots[--position] = digit_glyphs[value % DISPLAY_DECIMAL_BASE];
        value /= DISPLAY_DECIMAL_BASE;
    }
    do {
        frame.slots[--position] = digit_glyphs[value % DISPLAY_DECIMAL_BASE];
        value /= DISPLAY_DECIMAL_BASE;
    } while (value != 0U);
    if (fractional) {
        frame.slots[DISPLAY_DIGITS - 2U] |= eGlyphPoint;
    }
    if (negative) {
        frame.slots[--position] = eGlyphDash;
    }
    display_set_digits(frame);
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

void display_scan(void *context) {
    (void)context;
    if (scan_position == 0U) {
        active_frame = pending_frame;
    }

    bsp_display_write(
        active_frame.slots[scan_position],
        (uint8_t)(1U << scan_position)
    );
    scan_position++;
    if (scan_position == DISPLAY_SLOTS) {
        scan_position = 0U;
    }
}
/******************************************************************************/
