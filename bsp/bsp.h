/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    bsp.h
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-10-01
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef BSP_H
#define BSP_H

/********************************* Definitions ********************************/

/**
 * @enum EBspHeater
 * @brief Selects one of the two heater outputs.
 */
typedef enum EBspHeater {
    eBspHeater1 = 0,
    eBspHeater2 = 1
} EBspHeater_t;

/********************* Application Programming Interface *********************/

/** @fn bspInit */
void bspInit(void);

/** @fn bspHeaterOn */
void bspHeaterOn(EBspHeater_t heater);

/** @fn bspHeaterOff */
void bspHeaterOff(EBspHeater_t heater);

#endif // !BSP_H