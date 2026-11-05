/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    sensors_fake.h
 * @version 0.1.4
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef SENSORS_FAKE_H
#define SENSORS_FAKE_H
/******************************** Included files ******************************/
#include <stdbool.h>

#include "sensors.h"
/********************************* Definitions ********************************/
/**
 * @def SENSOR_FAKE_COUNT
 * @brief Number of simulated devices; indices are fixtures, not locations.
 */
#define SENSOR_FAKE_COUNT (2U)

/********************** Application Programming Interface *********************/
/**
 * @brief Get a fixture ROM even while its device is absent.
 * @param[in] index Zero-based fixture index.
 * @param[out] rom Complete DS18B20 ROM with a valid Dallas CRC.
 * @retval eResultOk ROM copied.
 * @retval eResultInvalidArgument NULL output or invalid index.
 * @details Does not require initialization; output is unchanged on failure.
 */
EResult_t sensors_fake_get_rom(uint8_t index, SSensorRom_t *rom);

/**
 * @brief Connect or disconnect one simulated device.
 * @param[in] index Zero-based fixture index.
 * @param[in] present True to connect, false to disconnect.
 * @retval eResultOk Presence updated.
 * @retval eResultInvalidArgument Invalid index.
 * @retval eResultNotReady sensors_init has not been called.
 */
EResult_t sensors_fake_set_present(uint8_t index, bool present);

/**
 * @brief Read the current connected fixture mask without changing the backend.
 * @param[out] presence Bit 0 for fixture 0, bit 1 for fixture 1.
 * @retval eResultOk Mask copied.
 * @retval eResultInvalidArgument Output is NULL.
 * @retval eResultNotReady sensors_init has not been called.
 * @details Output is unchanged on failure.
 */
EResult_t sensors_fake_get_presence(uint8_t *presence);

/**
 * @brief Set a simulated device's temperature, including while disconnected.
 * @param[in] index Zero-based fixture index.
 * @param[in] temperature Signed temperature in 1/16 degree Celsius units.
 * @retval eResultOk Temperature updated.
 * @retval eResultInvalidArgument Invalid index.
 * @retval eResultOutOfRange Temperature outside DS18B20 range, -55..125 C.
 * @retval eResultNotReady sensors_init has not been called.
 * @details No state changes on failure. Controls are cooperative/startup only.
 */
EResult_t sensors_fake_set_temperature(uint8_t index, int16_t temperature);
/******************************************************************************/
#endif // !SENSORS_FAKE_H
