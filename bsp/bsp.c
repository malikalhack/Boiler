/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    bsp.c
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-10-01
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <avr/io.h>

#include "bsp.h"

/********************************* Definitions ********************************/
#define BSP_HEATER_1_MASK    (_BV(PD5))
#define BSP_HEATER_2_MASK    (_BV(PD4))
#define BSP_HEATER_MASK      (BSP_HEATER_1_MASK | BSP_HEATER_2_MASK)

/***************************** Private functions *****************************/

static uint8_t bspGetHeaterMask(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = 0U;
    switch (heater) {
        case eBspHeater1:
            heater_mask = BSP_HEATER_1_MASK;
            break;
        case eBspHeater2:
            heater_mask = BSP_HEATER_2_MASK;
            break;
        default:
            break;
    }

    return heater_mask;
}

/********************* Application Programming Interface *********************/

/** @fn bspInit */
void bspInit(void) {
    PORTD |= BSP_HEATER_MASK;
    DDRD |= BSP_HEATER_MASK;
}

/*----------------------------------------------------------------------------*/

/** @fn bspHeaterOn */
void bspHeaterOn(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = bspGetHeaterMask(heater);
    if (heater_mask != 0U) {
        PORTD &= (uint8_t)~heater_mask;
        DDRD |= heater_mask;
    }
}

/*----------------------------------------------------------------------------*/

/** @fn bspHeaterOff */
void bspHeaterOff(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = bspGetHeaterMask(heater);
    if (heater_mask != 0U) {
        PORTD |= heater_mask;
        DDRD |= heater_mask;
    }
}
/******************************************************************************/