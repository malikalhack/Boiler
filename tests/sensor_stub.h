/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    sensor_stub.h
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef SENSOR_STUB_H
#define SENSOR_STUB_H
/******************************** Included files ******************************/
#include "sensors.h"
/********************************* Definitions ********************************/

/**
 * @def STUB_ALL_PRESENT
 * @brief Presence mask for all host-test devices.
 */
#define STUB_ALL_PRESENT ((1U << SENSOR_CAPACITY) - 1U)

/****************************** Module variables ******************************/

/** @brief Host-only complete ROMs used by configuration and menu unit tests. */
extern const SSensorRom_t stub_roms[SENSOR_CAPACITY];

/** @brief Host-only presence bits representing explicit test connections. */
extern uint8_t stub_presence;

/** @brief Host-only raw readings supplied to configuration and menu tests. */
extern int16_t stub_temperatures[SENSOR_CAPACITY];
/******************************************************************************/
#endif // !SENSOR_STUB_H
