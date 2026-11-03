/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    main.c
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-09-12
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "acrosched.h"
#include "acrosched_defs.h"
#include "display.h"
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

static void error_handler(void);

/********************** Application Programming Interface *********************/

/** @fn main */
int main(void) {
    bspInit();
    display_init();
    if (storageInit() != eResultOk) {
        IGNORE_RETURN(display_show_error(eErrorConfigInvalid));
    }

    CHECK_STATUS(acroInit(bspGetTick()));
    CHECK_STATUS(acroAddTask(
        eTaskDisplay, display_scan, NULL, ePeriodic, 1U, TASK_PRIORITY_DISPLAY
    ));

    bspStart();
    IGNORE_RETURN(acroRun());   /* should never return */

    bsp_display_write(0U, 0U);  /* turn off display */
    error_handler();            /* error handling if acroRun() returns */
}
/****************************** Private functions *****************************/

static void error_handler(void) {
    while (1) {
        /* halt */
    }
}
/******************************************************************************/
