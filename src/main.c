/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    main.c
 * @version 0.1.10
 * @authors Anton Chernov
 * @date    2026-09-12
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <string.h>

#include "acrosched.h"
#include "acrosched_defs.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "feedback.h"
#include "storage.h"
#include "bsp.h"
#include "tasks.h"
#include "sensors.h"
#include "menu.h"
/********************************* Definitions ********************************/

/**
 * @def CHECK_STATUS
 * @brief Checks the return value and, if unsuccessful, halts via error_handler.
 */
#define CHECK_STATUS(x) do { \
    if ((x) != eAcroOk) { \
        error_handler(); \
    } \
} while (0)

/**
 * @def CHECK_RESULT
 * @brief Halt on an unexpected application operation failure.
 */
#define CHECK_RESULT(x) do { \
    if ((x) != eResultOk) { \
        error_handler(); \
    } \
} while (0)

/***************************** Private prototypes *****************************/

/** @brief Silence the buzzer and halt on an unexpected internal failure. */
_Noreturn static void error_handler(void);

/**
 * @brief Advance acquisition and update the ordinary temperature diagnostic.
 * @param[in] context Unused scheduler parameter.
 * @note Service keeps display ownership while acquisition continues.
 */
static void check_sensors(void *context);

/** @brief Render the active diagnostic or verified first-channel reading. */
static void show_status(void);

/**
 * @brief Route button events to service while rejecting Enter chords.
 * @param[in] button Button producing the event.
 * @param[in] event Debounced transition or repeat.
 */
static void on_button(EButton_t button, EButtonEvent_t event);

/**
 * @brief Advance startup entry, assignment and verified configuration handoff.
 * @param[in] context Unused scheduler parameter.
 */
static void service_update(void *context);

/** @brief Evaluate the startup-only Left+Right hold without blocking. */
static void check_startup_service(void);

/**
 * @brief Initialize service with the current application diagnostic.
 * @param[in] error Diagnostic used for service entry and cancellation.
 */
static void init_menu(EError_t error);

/****************************** Module variables ******************************/

static SBoilerConfig_t active_config;
static bool config_loaded;
static EError_t active_error;
static bool startup_pending;
static bool startup_holding;
static bool service_active;
static AcroTick_t startup_last_tick;
static AcroTick_t startup_remaining;

/********************** Application Programming Interface *********************/

/**
 * @brief Initialize the safe interface firmware and run cooperative tasks.
 * @returns Never returns during normal operation.
 */
int main(void) {
    SBoilerConfig_t config;
    EError_t error = eErrorNone;

    bspInit();
    display_init();
    if (config_load(&config) != eResultOk) {
        error = eErrorConfigInvalid;
    }
    sensors_init();
    config_loaded = error == eErrorNone;
    if (config_loaded) {
        active_config = config;
        EResult_t result = config_check_sensors(&active_config, &error);
        if (result != eResultBusy) {
            CHECK_RESULT(result);
        }
    }
    active_error = error;
    init_menu(error);

    CHECK_STATUS(acroInit(bspGetTick()));
    CHECK_STATUS(acroAddTask(
        eTaskDisplay, display_scan, NULL, ePeriodic, 1U, TASK_PRIORITY_DISPLAY
    ));
    CHECK_STATUS(acroAddTask(
        eTaskFeedback, feedback_update, NULL, ePeriodic, 1U,
        TASK_PRIORITY_FEEDBACK
    ));
    CHECK_STATUS(acroAddTask(
        eTaskButtons, buttons_poll, NULL, ePeriodic, 1U,
        TASK_PRIORITY_BUTTONS
    ));
    CHECK_STATUS(acroAddTask(
        eTaskSensors, check_sensors, NULL, ePeriodic, 1U, TASK_PRIORITY_SENSORS
    ));
    CHECK_STATUS(acroAddTask(
        eTaskMenu, service_update, NULL, ePeriodic, 1U, TASK_PRIORITY_MENU
    ));

    buttons_init(on_button);
    /* Other inputs may still be settling; check the exact chord later. */
    startup_pending = (bsp_get_buttons() & (eButtonLeft | eButtonRight)) ==
                      (eButtonLeft | eButtonRight);
    startup_holding = false;
    startup_last_tick = bsp_get_time();
    startup_remaining = BUTTON_DEBOUNCE_TICKS;
    feedback_init(error != eErrorNone);
    show_status();
    bspStart();
    IGNORE_RETURN(acroRun());   /* should never return */

    bsp_display_write(0U, 0U);  /* turn off display */
    error_handler();            /* error handling if acroRun() returns */
}
/****************************** Private functions *****************************/

static void show_status(void) {
    SSensorRom_t rom;
    int16_t temperature;
    EResult_t result;

    CHECK_RESULT(feedback_set_mode(eLedMode3));
    feedback_set_error(active_error != eErrorNone);
    display_set_leds(DISPLAY_CHANNEL_LED_MASK, 0U);
    if (active_error != eErrorNone) {
        CHECK_RESULT(display_show_error(active_error));
    }
    else {
        memcpy(rom.bytes, active_config.sensor_rom[0], SENSOR_ROM_SIZE);
        result = sensors_read(rom, &temperature);
        if (result == eResultBusy) {
            display_show_dashes();
        }
        else {
            CHECK_RESULT(result);
            CHECK_RESULT(display_show_temperature(temperature));
            display_set_leds(DISPLAY_CHANNEL_LED_MASK, eLedChannel1);
        }
    }
}
/*----------------------------------------------------------------------------*/

static void check_sensors(void *context) {
    EError_t error;
    EResult_t result;

    (void)context;
    sensors_update(bsp_get_time());
    if (menu_is_active()) {
        return;
    }
    if (config_loaded) {
        result = config_check_sensors(&active_config, &error);
        if (result == eResultBusy) {
            return;
        }
        CHECK_RESULT(result);
        active_error = error;
        show_status();
    }
}
/*----------------------------------------------------------------------------*/

static void on_button(EButton_t button, EButtonEvent_t event) {
    if (
        button == eButtonEnter &&
        event == eButtonPressed &&
        buttons_get_state() != eButtonEnter
    ) {
        return;
    }
    menu_on_button(button, event);
}
/*----------------------------------------------------------------------------*/

static void service_update(void *context) {
    bool was_active = service_active || menu_is_active();

    check_startup_service();
    menu_update(context);
    if (menu_get_state() == eMenuApplied) {
        CHECK_RESULT(storageGet(&active_config));
        CHECK_RESULT(config_validate(&active_config));
        config_loaded = true;
        active_error = eErrorNone;
        EResult_t result = config_check_sensors(&active_config, &active_error);
        if (result != eResultBusy) {
            CHECK_RESULT(result);
        }
        init_menu(active_error);
    }
    service_active = menu_is_active();
    if (was_active && !service_active) {
        check_sensors(NULL);
        show_status();
    }
}
/*----------------------------------------------------------------------------*/

static void init_menu(EError_t error) {
    menu_init(error);
    service_active = false;
}
/*----------------------------------------------------------------------------*/

static void check_startup_service(void) {
    AcroTick_t now;
    AcroTick_t elapsed;
    uint8_t buttons;
    const uint8_t chord = eButtonLeft | eButtonRight;

    if (!startup_pending) {
        return;
    }
    now = bsp_get_time();
    elapsed = (AcroTick_t)(now - startup_last_tick);
    startup_last_tick = now;
    if (menu_is_active()) {
        startup_pending = false;
        return;
    }
    buttons = buttons_get_state();
    if (!startup_holding) {
        if (bsp_get_buttons() != chord) {
            if (elapsed >= startup_remaining) {
                startup_pending = false;
            }
            else {
                startup_remaining -= elapsed;
            }
        }
        else {
            startup_remaining = BUTTON_DEBOUNCE_TICKS;
            if (buttons == chord) {
                startup_holding = true;
                startup_remaining = MENU_STARTUP_HOLD_TICKS;
            }
        }
        return;
    }
    if (buttons != chord) {
        startup_pending = false;
        return;
    }
    if (bsp_get_buttons() != chord) {
        startup_remaining = MENU_STARTUP_HOLD_TICKS;
        return;
    }
    if (elapsed < startup_remaining) {
        startup_remaining -= elapsed;
    }
    else {
        startup_pending = false;
        CHECK_RESULT(menu_open(active_error));
    }
}
/*----------------------------------------------------------------------------*/

_Noreturn static void error_handler(void) {
    bsp_buzzer_set(false);
    while (1) {
        /* halt */
    }
}
/******************************************************************************/
