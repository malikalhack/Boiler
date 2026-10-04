/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    boiler_storage.h
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-10-04
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef BOILER_STORAGE_H
#define BOILER_STORAGE_H

/******************************** Included files ******************************/
#include <stdbool.h>
#include <stdint.h>
/********************************* Definitions ********************************/

/**
 * @def BOILER_CONFIG_SETTINGS_SIZE
 * @brief Number of opaque application-settings bytes stored in one record.
 */
#define BOILER_CONFIG_SETTINGS_SIZE (16U)

/********************************* Data types *********************************/

/**
 * @struct SBoilerConfig
 * @brief Application configuration persisted by the EEPROM storage module.
 */
typedef struct SBoilerConfig {
    uint8_t sensor_rom[2][8];
    uint8_t operating_profile;
    uint8_t application_settings[BOILER_CONFIG_SETTINGS_SIZE];
} SBoilerConfig_t;

/********************** Application Programming Interface *********************/

/** @brief Select and cache the newest valid EEPROM record. */
bool boilerStorageInit(void);

/** @brief Copy the cached configuration to the caller. */
bool boilerStorageGet(SBoilerConfig_t *config);

/** @brief Save and verify a configuration before committing its slot. */
bool boilerStorageSave(const SBoilerConfig_t *config);
/******************************************************************************/
#endif // !BOILER_STORAGE_H
