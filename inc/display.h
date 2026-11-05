/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    display.h
 * @version 0.1.5
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef DISPLAY_H
#define DISPLAY_H

/******************************** Included files ******************************/
#include <stdint.h>

#include "errors.h"
/********************************* Definitions ********************************/

/**
 * @def DISPLAY_DIGITS
 * @brief Number of seven-segment digits, in K1 to K4 order.
 */
#define DISPLAY_DIGITS (4U)

/**
 * @def DISPLAY_SLOTS
 * @brief Number of bus slots: four digits followed by the LED bank.
 */
#define DISPLAY_SLOTS (DISPLAY_DIGITS + 1U)

/**
 * @def DISPLAY_MODE3_LED_MASK
 * @brief HL7, segment C in K5, indicates the single-channel operating mode.
 */
#define DISPLAY_MODE3_LED_MASK (0x04U)

/**
 * @enum EDisplayGlyph
 * @brief Active-high glyphs with A..G on bits 0..6 and DP on bit 7.
 */
typedef enum EDisplayGlyph {
    eGlyphBlank = 0x00,
    eGlyph0     = 0x3F,
    eGlyph1     = 0x06,
    eGlyph2     = 0x5B,
    eGlyph3     = 0x4F,
    eGlyph4     = 0x66,
    eGlyph5     = 0x6D,
    eGlyph6     = 0x7D,
    eGlyph7     = 0x07,
    eGlyph8     = 0x7F,
    eGlyph9     = 0x6F,
    eGlyphDash  = 0x40,
    eGlyphE     = 0x79,
    eGlyphP     = 0x73,
    eGlyphC     = 0x39,
    eGlyphH     = 0x76,
    eGlyphF     = 0x71,
    eGlyphA     = 0x77,
    eGlyphV     = 0x3E,
    eGlyphN     = 0x54,
    eGlyphPoint = 0x80
} EDisplayGlyph_t;

/**
 * @struct SDisplayFrame
 * @brief Raw digit segments and LED bits sharing the PA0..PA7 bus.
 */
typedef struct SDisplayFrame {
    uint8_t slots[DISPLAY_SLOTS];
} SDisplayFrame_t;

/********************** Application Programming Interface *********************/
/**
 * @brief Reset scanning to K1, disable selections, and queue four dashes.
 * @details Call before starting the cooperative scheduler.
 */
void display_init(void);

/**
 * @brief Show four dashes without changing LED states.
 */
void display_show_dashes(void);

/**
 * @brief Queue the four digits from a frame, preserving pending LED bits.
 * @param[in] frame Digit patterns; its LED slot is ignored.
 */
void display_set_digits(SDisplayFrame_t frame);

/**
 * @brief Set decimal points independently of glyphs and LED bits.
 * @param[in] mask Bit 0 controls K1 point, bit 1 K2, and so on.
 */
void display_set_points(uint8_t mask);

/**
 * @brief Queue a complete frame for the next K1 slot.
 * @details Call only from cooperative tasks, not interrupt handlers.
 * Replaces any frame not yet displayed; digit and LED data latch together.
 * @param[in] frame Bus bytes in K1..K5 order; the last byte drives LEDs.
 */
void display_set_frame(SDisplayFrame_t frame);

/**
 * @brief Update selected LED bits without changing digits or other LEDs.
 * @param[in] mask LED bits owned by the caller.
 * @param[in] value New states of the selected bits.
 * @details The change latches at K1; use only from cooperative tasks/startup.
 */
void display_set_leds(uint8_t mask, uint8_t value);

/**
 * @brief Queue an E001..E009 screen while preserving the pending LED byte.
 * @param[in] error Application error code to display.
 * @returns eResultOk if queued; eResultInvalidArgument for zero/unknown codes.
 * @details Invalid codes leave the pending frame unchanged.
 * Call only from startup or cooperative tasks, not interrupt handlers.
 */
EResult_t display_show_error(EError_t error);

/**
 * @brief Show a DS18B20 reading (-55..125 C), preserving all LEDs.
 * @param[in] temperature Signed temperature in 1/16 degree Celsius units.
 * @details Round to tenths within -9.9..99.9, otherwise to whole degrees.
 * Halfway values round away from zero; leading zeros are blank.
 * @retval eResultOk Digits queued.
 * @retval eResultOutOfRange Invalid reading; pending frame is unchanged.
 */
EResult_t display_show_temperature(int16_t temperature);

/**
 * @brief Output one slot and advance cyclically from K1 through K5.
 * @details Call every system tick from the cooperative scheduler.
 * @param[in] context Unused scheduler parameter.
 */
void display_scan(void *context);
/******************************************************************************/
#endif // !DISPLAY_H
