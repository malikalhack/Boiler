/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    config.h
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef CONFIG_H
#define CONFIG_H
/******************************** Included files ******************************/
#include "storage.h"
/********************************* Definitions ********************************/

/**
 * @enum EProfile
 * @brief Persisted profile values; zero is invalid.
 */
typedef enum EProfile {
    eProfileSingle = 1, /**< One assigned sensor and heating channel. */
    eProfileTwo    = 2  /**< Two assigned sensors and heating channels. */
} EProfile_t;

/********************** Application Programming Interface *********************/

/**
 * @brief Validate profile and required ROM assignments, not bus presence.
 * @param[in] config Configuration to validate.
 * @retval eResultOk Required ROMs have DS18B20 family and valid CRC.
 * @retval eResultInvalidArgument config is NULL.
 * @retval eResultNoConfig Invalid profile, required ROM or duplicate ROMs.
 * @note The second ROM is unused in the single-channel profile.
 * Application settings remain opaque at this stage.
 */
EResult_t config_validate(const SBoilerConfig_t *config);

/**
 * @brief Load a validated configuration from EEPROM.
 * @param[out] config Destination, unchanged on failure.
 * @returns Validation or storage operation result.
 * @note Never writes EEPROM.
 */
EResult_t config_load(SBoilerConfig_t *config);

/**
 * @brief Check assigned devices against the selected sensor backend.
 * @param[in] config Configuration to check.
 * @param[out] error Application diagnostic, unchanged on operation failure.
 * @retval eResultOk Diagnostic is available, including eErrorNone.
 * @retval eResultInvalidArgument NULL argument.
 * @retval eResultBusy A real acquisition has not completed; error is unchanged.
 * @returns Other unexpected sensor backend failures are propagated.
 * @note Invalid assignments produce E001 before bus access; missing first
 * and second required devices produce E002 and E003 respectively. Unknown
 * devices produce E004. Missing assigned devices have priority over extras.
 * Real ROM/scratchpad CRC failures produce E005; bus, power, invalid data
 * and conversion-timeout or RAM readback failures produce E008.
 * Passing this check does not authorize heating.
 */
EResult_t config_check_sensors(const SBoilerConfig_t *config, EError_t *error);
/******************************************************************************/
#endif // !CONFIG_H
