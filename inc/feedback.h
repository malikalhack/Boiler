/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    feedback.h
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-11-04
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef FEEDBACK_H
#define FEEDBACK_H

/******************************** Included files ******************************/
#include <stdbool.h>

#include "boiler_config.h"
#include "display.h"

/********************************* Definitions ********************************/
/**
 * @def FEEDBACK_BEEP_TICKS
 * @brief Duration of the single startup beep, 100 ms.
 */
#define FEEDBACK_BEEP_TICKS     (BOILER_TICK_HZ / 10U)
/**
 * @def FEEDBACK_BLINK_TICKS
 * @brief Full mode-LED error blink period, one second with 50 percent duty.
 */
#define FEEDBACK_BLINK_TICKS    (BOILER_TICK_HZ)

/********************** Application Programming Interface *********************/

/**
 * @brief Start one short beep and initialize mode 3 in its ON phase.
 * @param[in] error True to blink HL7; false to keep it continuously on.
 * @note Call after successful task registration, before bspStart().
 */
void feedback_init(bool error);

/**
 * @brief Select the active mode LED without restarting the startup beep.
 * @param[in] mode One of eLedMode1, eLedMode2, eLedMode3 or eLedMode4.
 * @retval eResultOk Mode selected.
 * @retval eResultInvalidArgument Non-mode LED; state is unchanged.
 * @note A changed mode restarts its ON phase; other mode LEDs remain off.
 * Call after feedback_init, only from cooperative tasks.
 */
EResult_t feedback_set_mode(EDisplayLed_t mode);

/**
 * @brief Select blinking or continuous mode indication without another beep.
 * @param[in] error True for error indication; false for normal indication.
 * @note A changed state restarts the current mode LED in its ON phase.
 * Call only from cooperative tasks.
 */
void feedback_set_error(bool error);

/**
 * @brief Update buzzer and the current mode LED using elapsed system ticks.
 * @param[in] context Unused scheduler parameter.
 * @note Call every tick; gaps must be shorter than a full tick wrap.
 */
void feedback_update(void *context);
/******************************************************************************/
#endif // !FEEDBACK_H
