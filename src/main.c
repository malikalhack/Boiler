/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    main.c
 * @version 0.1.4
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
#endif /* SENSORS_USE_FAKE */

/****************************** Module variables ******************************/

#if (SENSORS_USE_FAKE == 1)
static SBoilerConfig_t active_config;
static bool config_loaded;
static EError_t active_error;
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
#endif /* SENSORS_USE_FAKE */

    buttons_init(NULL);
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
#endif /* SENSORS_USE_FAKE */

_Noreturn static void error_handler(void) {
    bsp_buzzer_set(false);
    while (1) {
        /* halt */
    }
}
/******************************************************************************/
