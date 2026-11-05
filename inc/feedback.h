/**
 * @file    feedback.h
 * @version 0.1.3
 * @authors Anton Chernov
 * @date    2026-11-04
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef FEEDBACK_H
#define FEEDBACK_H

/******************************** Included files ******************************/
#include <stdbool.h>

#include "boiler_config.h"

/********************************* Definitions ********************************/
/**
 * @def FEEDBACK_LED_MASK
 * @brief HL4 is connected to segment D, bit 3 of the K5 bus byte.
 */
#define FEEDBACK_LED_MASK       (0x08U)
/**
 * @def FEEDBACK_BEEP_TICKS
 * @brief Duration of the single startup beep, 100 ms.
 */
#define FEEDBACK_BEEP_TICKS     (BOILER_TICK_HZ / 10U)
/**
 * @def FEEDBACK_BLINK_TICKS
 * @brief Full HL4 error blink period, one second with 50 percent duty.
 */
#define FEEDBACK_BLINK_TICKS    (BOILER_TICK_HZ)

/********************** Application Programming Interface *********************/

/**
 * @brief Start one short beep and initialize HL4 in its ON phase.
 * @param[in] error True to blink HL4; false to keep it continuously on.
 * @note Call after successful task registration, before bspStart().
 */
void feedback_init(bool error);

/**
 * @brief Select blinking or continuous HL4 without restarting the beep.
 * @details A changed state restarts HL4 in its ON phase.
 * @param[in] error True for error indication; false for normal indication.
 * @note Call only from cooperative tasks.
 */
void feedback_set_error(bool error);

/**
 * @brief Update buzzer and HL4 using elapsed system ticks.
 * @param[in] context Unused scheduler parameter.
 * @note Call every tick; gaps must be shorter than a full tick wrap.
 */
void feedback_update(void *context);
/******************************************************************************/
#endif // !FEEDBACK_H
