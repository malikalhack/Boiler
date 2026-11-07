/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    menu.h
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef MENU_H
#define MENU_H
/******************************** Included files ******************************/
#include <stdbool.h>

#include "buttons.h"
#include "config.h"
/********************************* Definitions ********************************/

/**
 * @def MENU_STARTUP_HOLD_TICKS
 * @brief Debounced Left+Right startup hold required to open service, 3 s.
 */
#define MENU_STARTUP_HOLD_TICKS (3U * BOILER_TICK_HZ)

/**
 * @enum EMenuState
 * @brief Assignment prompts, cooperative operations and completion.
 */
typedef enum EMenuState {
    eMenuIdle = 0,   /**< Service does not own the display. */
    eMenuProfile,    /**< Select the operating profile. */
    eMenuSensor1,    /**< Prompt for the first assignment. */
    eMenuSensor2,    /**< Prompt for the second assignment. */
    eMenuCheck,      /**< Prompt to check all assigned devices. */
    eMenuSave,       /**< Prompt for explicit EEPROM confirmation. */
    eMenuMeasure1,   /**< First assignment acquisition is pending. */
    eMenuMeasure2,   /**< Second assignment acquisition is pending. */
    eMenuVerify,     /**< Assignment verification is pending. */
    eMenuPreSave,    /**< Fresh verification before writing is pending. */
    eMenuSaving,     /**< Cooperative EEPROM transaction is active. */
    eMenuFailure,    /**< Retryable failure owns the display. */
    eMenuApplied     /**< Verified record awaits application handoff. */
} EMenuState_t;

/********************** Application Programming Interface *********************/

/**
 * @brief Reset the assignment menu with the current application diagnostic.
 * @param[in] error Application diagnostic; Enter entry requires E001.
 * @note Does not touch EEPROM, reset sensors or change the idle screen.
 */
void menu_init(EError_t error);

/**
 * @brief Open service from a confirmed startup gesture at any diagnostic.
 * @param[in] error Application diagnostic to restore if service is cancelled.
 * @retval eResultOk Profile selection opened and both heaters disabled.
 * @retval eResultBusy Service already owns the display.
 * @retval eResultInvalidArgument Invalid diagnostic.
 * @note Does not modify the active configuration or EEPROM.
 */
EResult_t menu_open(EError_t error);

/**
 * @brief Handle debounced buttons without performing EEPROM writes.
 * @param[in] button Event source.
 * @param[in] event Press, release or Up/Down repeat.
 * @note Enter advances/acknowledges; Up/Down select profile; Left cancels
 * before writing; Right is inactive.
 * Enter requests a fresh cooperative bus acquisition before each check.
 * Once saving starts all keys are ignored until completion.
 */
void menu_on_button(EButton_t button, EButtonEvent_t event);

/**
 * @brief Advance discovery, checking and cooperative EEPROM save.
 * @param[in] context Unused scheduler parameter.
 * @note Call each tick. Heaters stay disabled throughout service.
 * Errors remain visible until Enter retries or Left cancels.
 * A verified save advances directly to the application handoff.
 */
void menu_update(void *context);

/**
 * @brief Read the current service state.
 * @returns Current assignment prompt or cooperative operation state.
 */
EMenuState_t menu_get_state(void);

/**
 * @brief Check whether service owns the display.
 * @returns True while ordinary diagnostics must yield the display.
 */
bool menu_is_active(void);

/**
 * @brief Return the last operation result for diagnostics.
 * @returns Specific backend/save result, including pending real acquisition.
 */
EResult_t menu_get_result(void);
/******************************************************************************/
#endif // !MENU_H
