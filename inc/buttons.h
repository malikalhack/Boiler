/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    buttons.h
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef BUTTONS_H
#define BUTTONS_H
/******************************** Included files ******************************/
#include <stdint.h>

#include "boiler_config.h"

/********************************* Definitions ********************************/
/**
 * @def BUTTON_DEBOUNCE_TICKS
 * @brief Stable input interval required for press and release, 20 ms.
 */
#define BUTTON_DEBOUNCE_TICKS    (BOILER_TICK_HZ / 50U)
/**
 * @def BUTTON_HOLD_TICKS
 * @brief Delay from confirmed press to first Up/Down repeat, three seconds.
 */
#define BUTTON_HOLD_TICKS        (3U * BOILER_TICK_HZ)
/**
 * @def BUTTON_REPEAT_TICKS
 * @brief Up/Down repeat interval after the hold delay, 500 ms.
 */
#define BUTTON_REPEAT_TICKS      (BOILER_TICK_HZ / 2U)

/**
 * @enum EButton
 * @brief Logical button bits, in PC3..PC7 order.
 */
typedef enum EButton {
    eButtonDown  = 0x01U, /**< Down input on PC3. */
    eButtonRight = 0x02U, /**< Right input on PC4. */
    eButtonUp    = 0x04U, /**< Up input on PC5. */
    eButtonLeft  = 0x08U, /**< Left input on PC6. */
    eButtonEnter = 0x10U  /**< Enter input on PC7. */
} EButton_t;

/**
 * @enum EButtonEvent
 * @brief Debounced transitions and held Up/Down repeats.
 */
typedef enum EButtonEvent {
    eButtonPressed  = 0, /**< Stable press after debounce. */
    eButtonReleased = 1, /**< Stable release after debounce. */
    eButtonRepeat   = 2  /**< Held Up/Down repeat after the hold interval. */
} EButtonEvent_t;

/**
 * @brief Receive one button event synchronously from the polling task.
 * @param[in] button Button producing the event.
 * @param[in] event Press, release or repeat.
 * @note Must be short, non-blocking and must not reenter the button driver.
 */
typedef void (*ButtonHandler_t)(EButton_t button, EButtonEvent_t event);

/********************** Application Programming Interface *********************/
/**
 * @brief Reset button state and sample initial inputs.
 * @param[in] handler Event receiver, or NULL for state-only polling.
 * @note Held startup buttons require debounce before a press is reported.
 * Call from startup or cooperative tasks, not from interrupts.
 */
void buttons_init(ButtonHandler_t handler);

/**
 * @brief Poll all buttons independently and deliver debounced events.
 * @param[in] context Unused scheduler parameter.
 * @note Call every tick. Elapsed time handles tick wrap; gaps must be
 * shorter than a full tick wrap. Late polls deliver at most one repeat per
 * button, without bursts of missed repeats. Raw release suspends repeats
 * immediately, before the debounced release event.
 */
void buttons_poll(void *context);

/**
 * @brief Return a bit mask of currently debounced pressed buttons.
 * @returns OR of EButton_t bits, or zero when none are pressed.
 * @note Call only from startup or cooperative tasks.
 */
uint8_t buttons_get_state(void);
/******************************************************************************/
#endif // !BUTTONS_H
