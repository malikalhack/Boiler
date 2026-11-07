/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    config.c
 * @version 0.1.6
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <stdbool.h>
#include <string.h>

#include "boiler_config.h"
#include "config.h"
#include "crc8.h"
#include "sensors.h"
#if (CONFIG_USE_TEST == 1)
#include "sensors_fake.h"
#endif /* CONFIG_USE_TEST */
/********************************* Definitions ********************************/

/**
 * @def CONFIG_SENSOR_COUNT
 * @brief Maximum number of assigned sensors in supported profiles.
 */
#define CONFIG_SENSOR_COUNT (2U)

/****************************** Private functions *****************************/

/**
 * @brief Validate a stored DS18B20 family code and complete ROM CRC.
 * @param[in] rom Complete stored ROM bytes.
 * @returns True for a supported address with a zero CRC remainder.
 */
static bool rom_valid(const uint8_t *rom) {
    return (rom[0] == SENSOR_FAMILY_DS18B20) &&
           (crc8(rom, SENSOR_ROM_SIZE) == 0U);
}

/********************** Application Programming Interface *********************/

EResult_t config_validate(const SBoilerConfig_t *config) {
    EResult_t ret_val = eResultNoConfig;

    if (config == NULL) {
        return eResultInvalidArgument;
    }
    if ((config->operating_profile == eProfileSingle) ||
        (config->operating_profile == eProfileTwo)) {
        if (rom_valid(config->sensor_rom[0])) {
            if ((config->operating_profile == eProfileSingle) ||
                (rom_valid(config->sensor_rom[1]) &&
                 (memcmp(config->sensor_rom[0], config->sensor_rom[1],
                         SENSOR_ROM_SIZE) != 0))) {
                ret_val = eResultOk;
            }
        }
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

EResult_t config_load(SBoilerConfig_t *config) {
    SBoilerConfig_t candidate = {0};
    EResult_t result;
#if (CONFIG_USE_TEST == 1)
    SSensorRom_t rom;
    uint8_t index;
    uint8_t required;
#endif /* CONFIG_USE_TEST */

    if (config == NULL) {
        return eResultInvalidArgument;
    }
#if (CONFIG_USE_TEST == 1)
    candidate.operating_profile = CONFIG_TEST_PROFILE;
    required = CONFIG_TEST_PROFILE == eProfileSingle ?
        1U : CONFIG_SENSOR_COUNT;
    for (index = 0U; index < required; index++) {
        result = sensors_fake_get_rom(index, &rom);
        if (result != eResultOk) {
            return result;
        }
        memcpy(candidate.sensor_rom[index], rom.bytes, SENSOR_ROM_SIZE);
    }
#else
    result = storageInit();
    if (result != eResultOk) {
        return result;
    }
    result = storageGet(&candidate);
    if (result != eResultOk) {
        return result;
    }
#endif /* CONFIG_USE_TEST */
    result = config_validate(&candidate);
    if (result == eResultOk) {
        *config = candidate;
    }
    return result;
}
/*----------------------------------------------------------------------------*/

EResult_t config_check_sensors(const SBoilerConfig_t *config, EError_t *error) {
    SSensorRom_t discovered[CONFIG_SENSOR_COUNT];
    SSensorRom_t rom;
    bool found[CONFIG_SENSOR_COUNT] = {false, false};
    uint8_t count;
    uint8_t index;
    uint8_t assignment;
    uint8_t required;
    bool unknown = false;
    bool matched;
    int16_t temperature;
    EResult_t result;

    if ((config == NULL) || (error == NULL)) {
        return eResultInvalidArgument;
    }
    if (config_validate(config) != eResultOk) {
        *error = eErrorConfigInvalid;
        return eResultOk;
    }
    required = config->operating_profile == eProfileSingle ?
        1U : CONFIG_SENSOR_COUNT;
    result = sensors_search(discovered, CONFIG_SENSOR_COUNT, &count);
    if (result == eResultOutOfRange) {
        *error = eErrorDeviceCount;
        return eResultOk;
    }
    if (result != eResultOk) {
#if (SENSORS_USE_FAKE == 0)
        if ((result == eResultCrc) || (result == eResultBus) ||
            (result == eResultPower) || (result == eResultTimeout) ||
            (result == eResultVerifyFailed)) {
            *error = sensors_error(result);
            return eResultOk;
        }
#endif
        return result;
    }
    for (index = 0U; index < count; index++) {
        matched = false;
        for (assignment = 0U; assignment < required; assignment++) {
            if (memcmp(discovered[index].bytes,
                       config->sensor_rom[assignment], SENSOR_ROM_SIZE) == 0) {
                found[assignment] = true;
                matched = true;
            }
        }
        unknown |= !matched;
    }
    if (!found[0]) {
        *error = eErrorSensor1;
    }
    else if ((required == CONFIG_SENSOR_COUNT) && !found[1]) {
        *error = eErrorSensor2;
    }
    else if (unknown) {
        *error = eErrorUnknownSensor;
    }
    else {
        for (assignment = 0U; assignment < required; assignment++) {
            memcpy(rom.bytes, config->sensor_rom[assignment], SENSOR_ROM_SIZE);
            result = sensors_read(rom, &temperature);
            if (result != eResultOk) {
                return result;
            }
        }
        *error = eErrorNone;
    }
    return eResultOk;
}
/******************************************************************************/
