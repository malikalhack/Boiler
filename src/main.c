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
#include "boiler_storage.h"
#include "bsp.h"
/********************** Application Programming Interface *********************/

/** @fn main */
int main(void) {
    EAcroStatus_t status;

    bspInit();
    (void)boilerStorageInit();

    status = acroInit(bspGetTick());
    if (status != eAcroOk) {
        for (;;) {}
    }

    bspStart();
    (void)acroRun();

    for (;;) {}
}
/******************************************************************************/
