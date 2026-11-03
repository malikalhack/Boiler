/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    boiler_config.h
 * @version 0.1.0
 * @authors Anton Chernov
 * @date    2026-10-03
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef BOILER_CONFIG_H
#define BOILER_CONFIG_H

/********************************* Definitions ********************************/

/**
 * @def BOILER_CPU_HZ
 * @brief CPU clock frequency in hertz.
 */
#define BOILER_CPU_HZ             (14745600UL)

/**
 * @def BOILER_TICK_HZ
 * @brief Target system tick frequency in hertz.
 */
#define BOILER_TICK_HZ            (1000UL)

/**
 * @def BOILER_USE_DS2480B
 * @brief Select the DS2480B 1-Wire driver when set to 1.
 */
#define BOILER_USE_DS2480B        (0U)

/**
 * @def BOILER_USE_SOFT_1WIRE
 * @brief Select the software 1-Wire driver when set to 1.
 */
#define BOILER_USE_SOFT_1WIRE     (1U)

/**
 * @def BOILER_USE_CPC1008N
 * @brief Select CPC1008N output control when set to 1.
 */
#define BOILER_USE_CPC1008N       (1U)

/**
 * @def BOILER_USE_MOC30XX
 * @brief Select MOC30xx output control when set to 1.
 */
#define BOILER_USE_MOC30XX        (0U)

#if ((BOILER_USE_DS2480B + BOILER_USE_SOFT_1WIRE) != 1U)
#error "Select exactly one 1-Wire driver"
#endif

#if ((BOILER_USE_CPC1008N + BOILER_USE_MOC30XX) != 1U)
#error "Select exactly one heater output driver"
#endif

#ifndef F_CPU
#define F_CPU                     BOILER_CPU_HZ
#endif

#if (F_CPU != BOILER_CPU_HZ)
#error "F_CPU does not match BOILER_CPU_HZ"
#endif
/******************************************************************************/
#endif // !BOILER_CONFIG_H
