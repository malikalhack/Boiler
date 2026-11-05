/**
 * @file    sensors.h
 * @version 0.1.4
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef SENSORS_H
#define SENSORS_H
/******************************** Included files ******************************/
#include <stdint.h>

#include "errors.h"
/********************************* Definitions ********************************/

/**
 * @def SENSOR_ROM_SIZE
 * @brief Size of a complete 1-Wire ROM, including family and CRC.
 */
#define SENSOR_ROM_SIZE     (8U)

/**
 * @def SENSOR_TEMP_SCALE
 * @brief Temperature units per degree Celsius, matching DS18B20 raw readings.
 */
#define SENSOR_TEMP_SCALE   (16)

typedef struct SSensorRom {
    uint8_t bytes[SENSOR_ROM_SIZE];
} SSensorRom_t;

/********************** Application Programming Interface *********************/

/**
 * @brief Initialize the selected sensor backend.
 * @details The fake backend resets to two present devices at 25 and 30 C.
 */
void sensors_init(void);

/**
 * @brief Discover present devices without assigning physical locations.
 * @param[out] roms Array receiving complete ROMs in backend discovery order.
 * @param[in] capacity Number of entries available in roms.
 * @param[out] count Number of discovered devices, including zero.
 * @retval eResultOk Discovery completed.
 * @retval eResultInvalidArgument NULL output or zero capacity.
 * @retval eResultOutOfRange Capacity cannot hold all discovered ROMs.
 * @retval eResultNotReady Backend has not been initialized.
 * @details Outputs remain unchanged on failure. Call from cooperative tasks
 * or startup only, never interrupts. Discovery order is not a location ID.
 */
EResult_t sensors_search(SSensorRom_t *roms, uint8_t capacity, uint8_t *count);

/**
 * @brief Read a present device addressed by its complete ROM.
 * @param[in] rom Complete ROM identifying the device.
 * @param[out] temperature Signed temperature in 1/16 degree Celsius units.
 * @retval eResultOk Temperature was read.
 * @retval eResultInvalidArgument Temperature output is NULL.
 * @retval eResultNotFound ROM is unknown or its device is absent.
 * @retval eResultNotReady Backend has not been initialized.
 * @details Output remains unchanged on failure. The fake returns immediately,
 * with no bus access or conversion delay. It does not authorize heating.
 */
EResult_t sensors_read(SSensorRom_t rom, int16_t *temperature);
/******************************************************************************/
#endif // !SENSORS_H
