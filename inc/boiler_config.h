/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    boiler_config.h
 * @version 0.1.3
 * @authors Anton Chernov
 * @date    2026-10-03
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef BOILER_CONFIG_H
#define BOILER_CONFIG_H

/********************************* Definitions ********************************/

/**
 * @def SENSORS_USE_FAKE
 * @brief Explicit test-backend opt-in; never enabled in a normal build.
 */
#ifndef SENSORS_USE_FAKE
#define SENSORS_USE_FAKE          (0U)
#endif /* SENSORS_USE_FAKE */

/**
 * @def CONFIG_USE_TEST
 * @brief Use fixture ROMs in RAM without accessing EEPROM; requires fake sensors.
 */
#ifndef CONFIG_USE_TEST
#define CONFIG_USE_TEST           (0U)
#endif /* CONFIG_USE_TEST */
/**
 * @def CONFIG_TEST_PROFILE
 * @brief Test profile: 1 single-channel, 2 two-channel.
 */
#ifndef CONFIG_TEST_PROFILE
#define CONFIG_TEST_PROFILE       (2U)
#endif /* CONFIG_TEST_PROFILE */
/**
 * @def SENSOR_FAKE_PRESENT_MASK
 * @brief Startup fixture presence: bit 0 first device, bit 1 second device.
 */
#ifndef SENSOR_FAKE_PRESENT_MASK
#define SENSOR_FAKE_PRESENT_MASK  (3U)
#endif /* SENSOR_FAKE_PRESENT_MASK */

#if ((CONFIG_USE_TEST != 0U) && (CONFIG_USE_TEST != 1U))
#error "CONFIG_USE_TEST must be 0 or 1"
#endif /* CONFIG_USE_TEST */

#if ((CONFIG_USE_TEST == 1U) && (SENSORS_USE_FAKE != 1U))
#error "RAM test configuration requires fake sensors"
#endif /* CONFIG_USE_TEST */

#if ((CONFIG_TEST_PROFILE != 1U) && (CONFIG_TEST_PROFILE != 2U))
#error "CONFIG_TEST_PROFILE must be 1 or 2"
#endif /* CONFIG_TEST_PROFILE */

#if ((SENSOR_FAKE_PRESENT_MASK < 0) || (SENSOR_FAKE_PRESENT_MASK > 3U))
#error "SENSOR_FAKE_PRESENT_MASK must be 0..3"
#endif /* SENSOR_FAKE_PRESENT_MASK */

#if ((SENSORS_USE_FAKE != 0U) && (SENSORS_USE_FAKE != 1U))
#error "SENSORS_USE_FAKE must be 0 or 1"
#endif /* SENSORS_USE_FAKE */

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
