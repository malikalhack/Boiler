/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    main.c
 * @version 0.1.1
 * @authors Anton Chernov
 * @date    2026-09-12
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "acrosched.h"
#include "acrosched_defs.h"
#include "display.h"
#include "feedback.h"
#include "storage.h"
#include "bsp.h"
#include "tasks.h"
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

/***************************** Private prototypes *****************************/

_Noreturn static void error_handler(void);

/********************** Application Programming Interface *********************/

/** @fn main */
int main(void) {
    bool config_error;

    bspInit();
    display_init();
    config_error = storageInit() != eResultOk;
    if (config_error) {
        IGNORE_RETURN(display_show_error(eErrorConfigInvalid));
    }

    CHECK_STATUS(acroInit(bspGetTick()));
    CHECK_STATUS(acroAddTask(
        eTaskDisplay, display_scan, NULL, ePeriodic, 1U, TASK_PRIORITY_DISPLAY
    ));
    CHECK_STATUS(acroAddTask(
        eTaskFeedback, feedback_update, NULL, ePeriodic, 1U,
        TASK_PRIORITY_FEEDBACK
    ));

    feedback_init(config_error);
    bspStart();
    IGNORE_RETURN(acroRun());   /* should never return */

    bsp_display_write(0U, 0U);  /* turn off display */
    error_handler();            /* error handling if acroRun() returns */
}
/****************************** Private functions *****************************/

_Noreturn static void error_handler(void) {
    bsp_buzzer_set(false);
    while (1) {
        /* halt */
    }
}
/******************************************************************************/
