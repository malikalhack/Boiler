/**
 * @file    main.c
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-09-12
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "acrosched.h"

/****************************** Module variables ******************************/
static volatile AcroTick_t system_tick = 0U;

/********************* Application Programming Interface *********************/

/** @fn main */
int main(void) {
    EAcroStatus_t status;

    status = acroInit(&system_tick);
    if (status != eAcroOk) {
        for (;;) {}
    }

    (void)acroRun();

    for (;;) {}
}
/******************************************************************************/
