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
#include "boiler_storage.h"
#include "bsp.h"
#include "tasks.h"
/********************** Application Programming Interface *********************/

/** @fn main */
int main(void) {
    EAcroStatus_t status;

    bspInit();
    display_init();
    if (boilerStorageInit() != eResultOk) {
        IGNORE_RETURN(display_show_error(eErrorConfigInvalid));
    }

    status = acroInit(bspGetTick());
    if (status == eAcroOk) {
        status = acroAddTask(
            eTaskDisplay, display_scan, NULL, ePeriodic, 1U,
            TASK_PRIORITY_DISPLAY);
    }
    if (status != eAcroOk) {
        for (;;) {}
    }

    bspStart();
    IGNORE_RETURN(acroRun());

    bsp_display_write(0U, 0U);
    for (;;) {}
}
/******************************************************************************/
