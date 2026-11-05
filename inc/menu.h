/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    menu.h
 * @version 0.1.6
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
    eMenuIdle = 0,
    eMenuProfile,
    eMenuSensor1,
    eMenuSensor2,
    eMenuCheck,
    eMenuSave,
    eMenuMeasure1,
    eMenuMeasure2,
    eMenuVerify,
    eMenuPreSave,
    eMenuSaving,
    eMenuFailure,
    eMenuApplied
} EMenuState_t;

/********************** Application Programming Interface *********************/

/**
 * @brief Reset the assignment menu with the current application diagnostic.
 * @param[in] error Application diagnostic; Enter entry is allowed only from E001.
 * @param[in] presence Initial fake presence mask, matching backend setup.
 * @details Available only with fake sensors and CONFIG_USE_TEST=0.
 * Does not touch EEPROM, reset sensors or change the displayed idle screen.
 */
void menu_init(EError_t error, uint8_t presence);

/**
 * @brief Open service from a confirmed startup gesture at any diagnostic.
 * @param[in] error Application diagnostic to restore if service is cancelled.
 * @param[in] presence Current fake presence mask, not the startup default.
 * @retval eResultOk Profile selection opened and both heaters disabled.
 * @retval eResultBusy Service already owns the display.
 * @retval eResultInvalidArgument Invalid diagnostic or presence mask.
 * @details Does not modify the active configuration or EEPROM.
 */
EResult_t menu_open(EError_t error, uint8_t presence);

/**
 * @brief Handle debounced buttons without performing EEPROM writes.
 * @param[in] button Event source.
 * @param[in] event Press, release or Up/Down repeat.
 * @details Enter advances/acknowledges; Up/Down select profile; Left cancels
 * before writing; Right cycles fake presence, shown by K1/K2 decimal points.
 * Once saving starts all keys are ignored until completion.
 */
void menu_on_button(EButton_t button, EButtonEvent_t event);

/**
 * @brief Advance discovery, checking and cooperative EEPROM save.
 * @param[in] context Unused scheduler parameter.
 * @details Call each tick. Heaters stay disabled throughout service.
 * Errors remain visible until Enter retries or Left cancels.
 * A verified save advances directly to the application handoff.
 */
void menu_update(void *context);

/** @brief Return the current service state. */
EMenuState_t menu_get_state(void);

/** @brief Return whether service owns the display and suspends diagnostics. */
bool menu_is_active(void);

/**
 * @brief Return the last operation result for diagnostics.
 * @returns Specific backend/save result; eResultBusy while saving.
 */
EResult_t menu_get_result(void);
/******************************************************************************/
#endif // !MENU_H
