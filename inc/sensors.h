/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    sensors.h
 * @version 0.1.6
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

/**
 * @def SENSOR_CAPACITY
 * @brief Maximum number of devices in a supported bus snapshot.
 */
#define SENSOR_CAPACITY       (2U)

/**
 * @def SENSOR_FAMILY_DS18B20
 * @brief Dallas family code of the supported temperature sensor.
 */
#define SENSOR_FAMILY_DS18B20 (0x28U)

/**
 * @def SENSOR_CONFIG_12BIT
 * @brief DS18B20 RAM configuration for 12-bit, 1/16 C measurements.
 */
#define SENSOR_CONFIG_12BIT   (0x7FU)

/**
 * @def SENSOR_CONVERSION_TICKS
 * @brief Maximum normal 12-bit conversion duration at the 1 ms tick rate.
 */
#define SENSOR_CONVERSION_TICKS (750U)

/**
 * @def SENSOR_TIMEOUT_TICKS
 * @brief Conversion completion deadline at the 1 ms tick rate.
 */
#define SENSOR_TIMEOUT_TICKS    (800U)

/**
 * @struct SSensorRom
 * @brief Complete 64-bit device address, including family and Dallas CRC.
 */
typedef struct SSensorRom {
    uint8_t bytes[SENSOR_ROM_SIZE]; /**< Address bytes in wire order. */
} SSensorRom_t;

/********************** Application Programming Interface *********************/

/**
 * @brief Map an acquisition failure to its display diagnostic.
 * @param[in] result Failed acquisition result, not eResultOk or eResultBusy.
 * @returns E005 for CRC, E006 for capacity, or E008 for other failures.
 */
static inline EError_t sensors_error(EResult_t result) {
    return result == eResultCrc ? eErrorCrc :
        result == eResultOutOfRange ? eErrorDeviceCount : eErrorSoft1Wire;
}

/**
 * @brief Initialize the selected sensor backend.
 * @note The fake backend resets to two present devices at 25 and 30 C.
 * The real backend starts acquisition and verifies 12-bit RAM configuration.
 */
void sensors_init(void);

/**
 * @brief Advance the real backend by one reset, byte or search triplet.
 * @param[in] now Current 1 ms system tick; poll once per tick.
 * @note Call sensors_init() before polling or registering the polling task.
 * Conversion waits are non-blocking. Poll gaps must be shorter than one
 * 16-bit tick wrap. Available for the real software 1-Wire backend only.
 */
void sensors_update(uint16_t now);

/**
 * @brief Request a fresh real-bus snapshot, discarding the previous result.
 * @note Call only after sensors_init(). Used before service measurements;
 * never writes sensor EEPROM.
 */
void sensors_refresh(void);

/**
 * @brief Discover present devices without assigning physical locations.
 * @param[out] roms Array receiving complete ROMs in backend discovery order.
 * @param[in] capacity Number of entries available in roms.
 * @param[out] count Number of discovered devices, including zero.
 * @retval eResultOk Discovery completed.
 * @retval eResultInvalidArgument NULL output or zero capacity.
 * @retval eResultOutOfRange Capacity cannot hold all discovered ROMs.
 * @retval eResultNotReady Backend has not been initialized.
 * @retval eResultBusy Real backend has not published a completed snapshot.
 * @retval eResultCrc A real ROM or scratchpad failed CRC.
 * @retval eResultPower A real sensor uses unsupported parasite power.
 * @retval eResultBus Real bus levels or scratchpad data are invalid.
 * @retval eResultTimeout A real conversion did not finish in time.
 * @retval eResultVerifyFailed Real RAM configuration readback failed.
 * @note Outputs remain unchanged on failure. Call from cooperative tasks
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
 * @retval eResultBusy The first or explicitly refreshed snapshot is pending.
 * @returns Other failures from the latest real acquisition are propagated.
 * @note Real reads use the last fully checked snapshot, never power-on
 * scratchpad data before a completed conversion. Real errors invalidate that
 * snapshot; refresh yields Busy until the next complete acquisition.
 * Output remains unchanged on failure. The fake returns immediately,
 * with no bus access or conversion delay. It does not authorize heating.
 */
EResult_t sensors_read(SSensorRom_t rom, int16_t *temperature);
/******************************************************************************/
#endif // !SENSORS_H
