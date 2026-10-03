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
#include "bsp.h"

/****************************** Module variables ******************************/

/********************** Application Programming Interface *********************/

/** @fn main */
int main(void) {
    EAcroStatus_t status;

    bspInit();

    status = acroInit(bspGetTick());
    if (status != eAcroOk) {
        for (;;) {}
    }

    bspStart();
    (void)acroRun();

    for (;;) {}
}
/******************************************************************************/
