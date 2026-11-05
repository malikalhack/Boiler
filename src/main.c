/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    main.c
 * @version 0.1.5
 * @authors Anton Chernov
 * @date    2026-09-12
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "acrosched.h"
#include "acrosched_defs.h"
#include "buttons.h"
#include "config.h"
#include "display.h"
#include "feedback.h"
#include "storage.h"
#include "bsp.h"
#include "tasks.h"
#if (SENSORS_USE_FAKE == 1)
#include "sensors_fake.h"
#if (CONFIG_USE_TEST == 0)
#include "menu.h"
#endif /* CONFIG_USE_TEST */
#endif /* SENSORS_USE_FAKE */
/********************************* Definitions ********************************/

/*
 * @def CHECK_STATUS
 * @brief Checks the return value and, if unsuccessful, halts via error_handler.
 */
#define CHECK_STATUS(x) do { \
    if ((x) != eAcroOk) { \
        error_handler(); \
    } \
} while (0)

/*
 * @def CHECK_RESULT
 * @brief Halt on an unexpected application operation failure.
 */
#define CHECK_RESULT(x) do { \
    if ((x) != eResultOk) { \
        error_handler(); \
    } \
} while (0)

/***************************** Private prototypes *****************************/

_Noreturn static void error_handler(void);
#if (SENSORS_USE_FAKE == 1)
static void check_sensors(void *context);
#if (CONFIG_USE_TEST == 0)
static void on_button(EButton_t button, EButtonEvent_t event);
static void service_update(void *context);
static void check_startup_service(void);
static void init_menu(EError_t error);
#endif /* CONFIG_USE_TEST */
#endif /* SENSORS_USE_FAKE */

/****************************** Module variables ******************************/

#if (SENSORS_USE_FAKE == 1)
static SBoilerConfig_t active_config;
static bool config_loaded;
static EError_t active_error;
#if (CONFIG_USE_TEST == 0)
static bool startup_pending;
static bool startup_holding;
static AcroTick_t startup_last_tick;
static AcroTick_t startup_remaining;
#endif /* CONFIG_USE_TEST */
#endif /* SENSORS_USE_FAKE */

/********************** Application Programming Interface *********************/

/** @fn main */
int main(void) {
    SBoilerConfig_t config;
    EError_t error = eErrorNone;

    bspInit();
    display_init();
    if (config_load(&config) != eResultOk) {
        error = eErrorConfigInvalid;
    }
#if (SENSORS_USE_FAKE == 1)
    sensors_init();
    CHECK_RESULT(sensors_fake_set_present(
        0U, (SENSOR_FAKE_PRESENT_MASK & 1U) != 0U));
    CHECK_RESULT(sensors_fake_set_present(
        1U, (SENSOR_FAKE_PRESENT_MASK & 2U) != 0U));
    config_loaded = error == eErrorNone;
    if (config_loaded) {
        active_config = config;
        CHECK_RESULT(config_check_sensors(&active_config, &error));
    }
    active_error = error;
#if (CONFIG_USE_TEST == 0)
    init_menu(error);
#endif /* CONFIG_USE_TEST */
#endif /* SENSORS_USE_FAKE */
    if (error != eErrorNone) {
        CHECK_RESULT(display_show_error(error));
    }

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
#if (SENSORS_USE_FAKE == 1)
    CHECK_STATUS(acroAddTask(
        eTaskSensors, check_sensors, NULL, ePeriodic,
        BOILER_TICK_HZ / 10U, TASK_PRIORITY_SENSORS
    ));
#if (CONFIG_USE_TEST == 0)
    CHECK_STATUS(acroAddTask(
        eTaskMenu, service_update, NULL, ePeriodic, 1U, TASK_PRIORITY_MENU
    ));
#endif /* CONFIG_USE_TEST */
#endif /* SENSORS_USE_FAKE */

#if ((SENSORS_USE_FAKE == 1) && (CONFIG_USE_TEST == 0))
    buttons_init(on_button);
    /* Other inputs may still be settling; check the exact chord later. */
    startup_pending = (bsp_get_buttons() & (eButtonLeft | eButtonRight)) ==
                      (eButtonLeft | eButtonRight);
    startup_holding = false;
    startup_last_tick = bsp_get_time();
    startup_remaining = BUTTON_DEBOUNCE_TICKS;
#else
    buttons_init(NULL);
#endif /* SENSORS_USE_FAKE && CONFIG_USE_TEST */
    feedback_init(error != eErrorNone);
    bspStart();
    IGNORE_RETURN(acroRun());   /* should never return */

    bsp_display_write(0U, 0U);  /* turn off display */
    error_handler();            /* error handling if acroRun() returns */
}
/****************************** Private functions *****************************/

#if (SENSORS_USE_FAKE == 1)
static void check_sensors(void *context) {
    EError_t error;

    (void)context;
#if (CONFIG_USE_TEST == 0)
    if (menu_is_active()) {
        return;
    }
#endif /* CONFIG_USE_TEST */
    if (config_loaded) {
        CHECK_RESULT(config_check_sensors(&active_config, &error));
        if (error != active_error) {
            if (error == eErrorNone) {
                display_show_dashes();
            }
            else {
                CHECK_RESULT(display_show_error(error));
            }
            feedback_set_error(error != eErrorNone);
            active_error = error;
        }
    }
}
/*----------------------------------------------------------------------------*/
#if (CONFIG_USE_TEST == 0)
static void on_button(EButton_t button, EButtonEvent_t event) {
    if ((button == eButtonEnter) && (event == eButtonPressed) &&
        (buttons_get_state() != eButtonEnter)) {
        return;
    }
    menu_on_button(button, event);
}
/*----------------------------------------------------------------------------*/

static void service_update(void *context) {
    check_startup_service();
    menu_update(context);
    if (menu_get_state() == eMenuApplied) {
        CHECK_RESULT(storageGet(&active_config));
        CHECK_RESULT(config_validate(&active_config));
        config_loaded = true;
        CHECK_RESULT(config_check_sensors(&active_config, &active_error));
        if (active_error == eErrorNone) {
            display_show_dashes();
        }
        else {
            CHECK_RESULT(display_show_error(active_error));
        }
        feedback_set_error(active_error != eErrorNone);
        init_menu(active_error);
    }
}
/*----------------------------------------------------------------------------*/

static void init_menu(EError_t error) {
    uint8_t presence;

    CHECK_RESULT(sensors_fake_get_presence(&presence));
    menu_init(error, presence);
}
/*----------------------------------------------------------------------------*/

static void check_startup_service(void) {
    AcroTick_t now;
    AcroTick_t elapsed;
    uint8_t presence;
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
        CHECK_RESULT(sensors_fake_get_presence(&presence));
        CHECK_RESULT(menu_open(active_error, presence));
    }
}
/*----------------------------------------------------------------------------*/
#endif /* CONFIG_USE_TEST */
#endif /* SENSORS_USE_FAKE */

_Noreturn static void error_handler(void) {
    bsp_buzzer_set(false);
    while (1) {
        /* halt */
    }
}
/******************************************************************************/
