/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    errors.h
 * @version 0.1.7
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef ERRORS_H
#define ERRORS_H

/********************************* Definitions ********************************/

/**
 * @enum EResult
 * @brief Operation outcome, distinct from application diagnostic codes.
 * @note Compare explicitly with eResultOk; success is zero, not true.
 */
typedef enum EResult {
    eResultOk              = 0, /**< Operation completed successfully. */
    eResultInvalidArgument = 1, /**< NULL buffer, zero size, or unknown code. */
    eResultOutOfRange      = 2, /**< Range, capacity or value limit exceeded. */
    eResultNoConfig        = 3, /**< No valid configuration is available. */
    eResultReadFailed      = 4, /**< EEPROM backend could not read data. */
    eResultWriteFailed     = 5, /**< EEPROM backend could not write data. */
    eResultVerifyFailed    = 6, /**< Readback does not match requested data. */
    eResultNotFound        = 7, /**< Addressed device is absent or unknown. */
    eResultNotReady        = 8, /**< Module has not been initialized. */
    eResultBusy            = 9, /**< An operation is still in progress. */
    eResultCrc             = 10, /**< Sensor ROM or scratchpad CRC failed. */
    eResultBus             = 11, /**< Bus levels or sensor data are invalid. */
    eResultTimeout         = 12, /**< Sensor conversion did not finish. */
    eResultPower           = 13  /**< Parasite power is unsupported. */
} EResult_t;

/**
 * @enum EError
 * @brief Application diagnostics matching the displayed E001..E009 codes.
 */
typedef enum EError {
    eErrorNone          = 0, /**< No application error. */
    eErrorConfigInvalid = 1, /**< E001: missing or invalid configuration. */
    eErrorSensor1       = 2, /**< E002: sensor at location 1 is missing. */
    eErrorSensor2       = 3, /**< E003: sensor at location 2 is missing. */
    eErrorUnknownSensor = 4, /**< E004: unknown sensor detected. */
    eErrorCrc           = 5, /**< E005: ROM or scratchpad CRC failure. */
    eErrorDeviceCount   = 6, /**< E006: invalid number of 1-Wire devices. */
    eErrorDs2480b       = 7, /**< E007: DS2480B unavailable or uninitialized. */
    eErrorSoft1Wire     = 8, /**< E008: direct 1-Wire interface failure. */
    eErrorWatchdog      = 9  /**< E009: watchdog reset detected. */
} EError_t;
/******************************************************************************/
#endif // !ERRORS_H
