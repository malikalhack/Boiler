/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    storage.h
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-10-04
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef STORAGE_H
#define STORAGE_H

/******************************** Included files ******************************/
#include <stdbool.h>
#include <stdint.h>

#include "errors.h"
/********************************* Definitions ********************************/

/**
 * @def CONFIG_SETTINGS_SIZE
 * @brief Number of opaque application-settings bytes stored in one record.
 */
#define CONFIG_SETTINGS_SIZE (16U)

/********************************* Data types *********************************/

/**
 * @struct SBoilerConfig
 * @brief Application configuration persisted by the EEPROM storage module.
 */
typedef struct SBoilerConfig {
    uint8_t sensor_rom[2][8];
    uint8_t operating_profile;
    uint8_t application_settings[CONFIG_SETTINGS_SIZE];
} SBoilerConfig_t;

/********************** Application Programming Interface *********************/

/**
 * @brief Select and cache the newest valid EEPROM record.
 * @retval eResultOk A valid record is cached.
 * @retval eResultNoConfig Both records are absent or invalid.
 * @retval eResultOutOfRange The configuration slots exceed EEPROM capacity.
 * @retval eResultBusy A cooperative save is still in progress.
 * @returns Other failures are propagated from the EEPROM backend.
 */
EResult_t storageInit(void);

/**
 * @brief Copy the cached configuration to the caller.
 * @param[out] config Destination configuration.
 * @retval eResultOk The cached configuration was copied.
 * @retval eResultInvalidArgument Destination is NULL.
 * @retval eResultNoConfig No valid configuration is cached.
 */
EResult_t storageGet(SBoilerConfig_t *config);

/**
 * @brief Save and verify a configuration before committing its slot.
 * @param[in] config Configuration to persist.
 * @retval eResultOk The new record was committed and verified.
 * @retval eResultInvalidArgument Configuration is NULL.
 * @retval eResultOutOfRange The configuration slots exceed EEPROM capacity.
 * @retval eResultVerifyFailed Record readback or integrity checks failed.
 * @retval eResultBusy A cooperative save is still in progress.
 * @returns Other failures are propagated from the EEPROM backend.
 */
EResult_t storageSave(const SBoilerConfig_t *config);

/**
 * @brief Begin a cooperative save of a complete configuration.
 * @param[in] config Configuration copied into the transaction buffer.
 * @retval eResultOk Transaction accepted; call storage_save_poll each tick.
 * @retval eResultBusy Another save or hardware write is in progress.
 * @returns Other errors follow storageSave.
 * @note Does not write during this call. Do not call storageInit or
 * storageSave during the transaction. Once started, poll until completion.
 */
EResult_t storage_save_begin(const SBoilerConfig_t *config);

/**
 * @brief Advance a save without waiting for the EEPROM write engine.
 * @retval eResultBusy Transaction is still in progress.
 * @retval eResultOk Complete record was committed and verified.
 * @retval eResultNotReady No transaction has been started.
 * @returns Specific EEPROM or verification failure on error.
 * @note Each call writes at most one byte or performs one read/verify step.
 * Failed saves leave the cached active configuration unchanged. The final
 * result remains available until the next transaction.
 */
EResult_t storage_save_poll(void);
/******************************************************************************/
#endif // !STORAGE_H
