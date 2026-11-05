/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    storage.c
 * @version 0.1.1
 * @authors Anton Chernov
 * @date    2026-10-04
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <string.h>

#include "bsp.h"
#include "storage.h"

/********************************* Definitions ********************************/
/**
 * @def STORAGE_MAGIC_0
 * @brief First magic byte identifying an application record.
 */
#define STORAGE_MAGIC_0             (0xB0U)
/**
 * @def STORAGE_MAGIC_1
 * @brief Second magic byte identifying an application record.
 */
#define STORAGE_MAGIC_1             (0x1EU)
/**
 * @def STORAGE_FORMAT
 * @brief Current EEPROM record format version.
 */
#define STORAGE_FORMAT              (1U)
/**
 * @def STORAGE_COMMIT_MARKER
 * @brief Marker written only after a record has been verified.
 */
#define STORAGE_COMMIT_MARKER       (0xA5U)
/**
 * @def STORAGE_EMPTY_MARKER
 * @brief Marker value used to invalidate the target slot before writing.
 */
#define STORAGE_EMPTY_MARKER        (0x00U)
/**
 * @def STORAGE_SLOT_COUNT
 * @brief Number of EEPROM configuration slots.
 */
#define STORAGE_SLOT_COUNT          (2U)
/**
 * @def STORAGE_EEPROM_START
 * @brief First EEPROM address reserved for application configuration.
 */
#define STORAGE_EEPROM_START        (0x20U)
/**
 * @def STORAGE_SEQUENCE_HALF
 * @brief Half-range used to compare wrapping 16-bit sequence numbers.
 */
#define STORAGE_SEQUENCE_HALF       (0x8000U)
/**
 * @def STORAGE_MAGIC_0_OFFSET
 * @brief Record offset of the first magic byte.
 */
#define STORAGE_MAGIC_0_OFFSET      (0U)
/**
 * @def STORAGE_MAGIC_1_OFFSET
 * @brief Record offset of the second magic byte.
 */
#define STORAGE_MAGIC_1_OFFSET      (1U)
/**
 * @def STORAGE_FORMAT_OFFSET
 * @brief Record offset of the format version.
 */
#define STORAGE_FORMAT_OFFSET       (2U)
/**
 * @def STORAGE_SEQUENCE_OFFSET
 * @brief Record offset of the two-byte sequence number.
 */
#define STORAGE_SEQUENCE_OFFSET     (3U)
/**
 * @def STORAGE_SENSOR_OFFSET
 * @brief Record offset of the first sensor ROM code.
 */
#define STORAGE_SENSOR_OFFSET       (5U)
/**
 * @def STORAGE_PROFILE_OFFSET
 * @brief Record offset of the operating profile.
 */
#define STORAGE_PROFILE_OFFSET      (21U)
/**
 * @def STORAGE_SETTINGS_OFFSET
 * @brief Record offset of the application settings payload.
 */
#define STORAGE_SETTINGS_OFFSET     (22U)
/**
 * @def STORAGE_CRC_OFFSET
 * @brief Record offset of the CRC-16 field.
 */
#define STORAGE_CRC_OFFSET \
    (STORAGE_SETTINGS_OFFSET + CONFIG_SETTINGS_SIZE)
/**
 * @def STORAGE_MARKER_OFFSET
 * @brief Record offset of the final commit marker.
 */
#define STORAGE_MARKER_OFFSET \
    (STORAGE_CRC_OFFSET + 2U)
/**
 * @def STORAGE_RECORD_SIZE
 * @brief Total size of one EEPROM configuration record in bytes.
 */
#define STORAGE_RECORD_SIZE \
    (STORAGE_MARKER_OFFSET + 1U)

/****************************** Module variables ******************************/

static SBoilerConfig_t current_config; ///< Currently selected configuration.
static uint8_t active_slot; ///< EEPROM slot containing current_config.
static uint16_t current_sequence; ///< Sequence number of current_config.
static bool has_config; ///< True when a valid record is cached.
static bool storage_initialized; ///< True after the EEPROM scan has run.

/***************************** Private prototypes *****************************/

/**
 * @brief Calculate the CRC-16 of a byte range.
 * @param[in] data First byte of the data range.
 * @param[in] size Number of bytes to process.
 * @returns CRC-16 value for the data range.
 */
static uint16_t boilerStorageCrc(const uint8_t *data, uint8_t size);

/**
 * @brief Read the sequence number from an encoded record.
 * @param[in] record Encoded configuration record.
 * @returns The record's 16-bit sequence number.
 */
static uint16_t boilerStorageReadSequence(const uint8_t *record);

/**
 * @brief Get the starting EEPROM address of a slot.
 * @param[in] slot Configuration slot index.
 * @returns EEPROM address at which the slot begins.
 */
static uint16_t boilerStorageGetAddress(uint8_t slot);

/**
 * @brief Check that EEPROM can hold both configuration slots.
 * @returns true if both slots fit in EEPROM; otherwise false.
 */
static bool boilerStorageLayoutValid(void);

/**
 * @brief Validate a record's magic, format, commit marker, and CRC.
 * @param[in] record Encoded configuration record to validate.
 * @returns true if the record is valid; otherwise false.
 */
static bool boilerStorageRecordValid(const uint8_t *record);

/**
 * @brief Compare sequence numbers using 16-bit wraparound ordering.
 * @param[in] candidate Sequence number being tested.
 * @param[in] reference Sequence number to compare against.
 * @returns true if candidate is newer than reference; otherwise false.
 */
static bool boilerStorageSequenceNewer(uint16_t candidate, uint16_t reference);

/**
 * @brief Read a complete record from the selected EEPROM slot.
 * @param[in] slot Configuration slot index.
 * @param[out] record Buffer receiving the encoded record.
 * @returns EEPROM backend result without discarding the failure cause.
 */
static EResult_t boilerStorageReadRecord(uint8_t slot, uint8_t *record);

/**
 * @brief Write record bytes up to, but not including, the commit marker.
 * @param[in] slot Configuration slot index.
 * @param[in] record Encoded record to write.
 * @returns EEPROM backend result without discarding the failure cause.
 */
static EResult_t boilerStorageWriteRecord(uint8_t slot, const uint8_t *record);

/**
 * @brief Encode a configuration and sequence number into a record.
 * @param[in] config Configuration data to encode.
 * @param[in] sequence Sequence number to store.
 * @param[out] record Buffer receiving the encoded record.
 */
static void boilerStorageEncode(
    const SBoilerConfig_t *config,
    uint16_t sequence,
    uint8_t *record
);

/**
 * @brief Decode configuration fields from a validated record.
 * @param[in] record Encoded configuration record.
 * @param[out] config Destination for decoded configuration fields.
 */
static void boilerStorageDecode(const uint8_t *record, SBoilerConfig_t *config);

/****************************** Private functions *****************************/

static uint16_t boilerStorageCrc(const uint8_t *data, uint8_t size) {
    uint16_t crc;
    uint8_t byte_index;
    uint8_t bit_index;

    crc = 0xFFFFU;
    for (byte_index = 0U; byte_index < size; byte_index++) {
        crc ^= (uint16_t)data[byte_index] << 8U;
        for (bit_index = 0U; bit_index < 8U; bit_index++) {
            if ((crc & 0x8000U) != 0U) {
                crc = (uint16_t)((crc << 1U) ^ 0x1021U);
            }
            else {
                crc <<= 1U;
            }
        }
    }

    return crc;
}
/*----------------------------------------------------------------------------*/

static uint16_t boilerStorageReadSequence(const uint8_t *record) {
    uint16_t sequence;

    sequence = (uint16_t)record[STORAGE_SEQUENCE_OFFSET] << 8U;
    sequence |= record[STORAGE_SEQUENCE_OFFSET + 1U];
    return sequence;
}
/*----------------------------------------------------------------------------*/

static uint16_t boilerStorageGetAddress(uint8_t slot) {
    uint16_t address;

    address = STORAGE_EEPROM_START +
              ((uint16_t)slot * STORAGE_RECORD_SIZE);
    return address;
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageLayoutValid(void) {
    uint32_t required_size;

    required_size = STORAGE_EEPROM_START +
                    (STORAGE_SLOT_COUNT *
                     STORAGE_RECORD_SIZE);
    return required_size <= bspEepromSize();
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageRecordValid(const uint8_t *record) {
    uint16_t stored_crc;
    uint16_t calculated_crc;

    stored_crc = (uint16_t)record[STORAGE_CRC_OFFSET] << 8U;
    stored_crc |= record[STORAGE_CRC_OFFSET + 1U];
    calculated_crc = boilerStorageCrc(record, STORAGE_CRC_OFFSET);

    return (record[STORAGE_MAGIC_0_OFFSET] ==
                STORAGE_MAGIC_0) &&
           (record[STORAGE_MAGIC_1_OFFSET] ==
                STORAGE_MAGIC_1) &&
           (record[STORAGE_FORMAT_OFFSET] == STORAGE_FORMAT) &&
           (record[STORAGE_MARKER_OFFSET] ==
                STORAGE_COMMIT_MARKER) &&
           (stored_crc == calculated_crc);
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageSequenceNewer(uint16_t candidate, uint16_t reference) {
    uint16_t difference;

    difference = (uint16_t)(candidate - reference);
    return (candidate != reference) &&
           (difference < STORAGE_SEQUENCE_HALF);
}
/*----------------------------------------------------------------------------*/

static EResult_t boilerStorageReadRecord(uint8_t slot, uint8_t *record) {
    return bspEepromRead(
        boilerStorageGetAddress(slot),
        record,
        STORAGE_RECORD_SIZE
    );
}
/*----------------------------------------------------------------------------*/

static EResult_t boilerStorageWriteRecord(uint8_t slot, const uint8_t *record) {
    return bspEepromUpdate(
        boilerStorageGetAddress(slot),
        record,
        STORAGE_MARKER_OFFSET
    );
}
/*----------------------------------------------------------------------------*/

static void boilerStorageEncode(
    const SBoilerConfig_t *config,
    uint16_t sequence,
    uint8_t *record
) {
    uint16_t crc;

    record[STORAGE_MAGIC_0_OFFSET] = STORAGE_MAGIC_0;
    record[STORAGE_MAGIC_1_OFFSET] = STORAGE_MAGIC_1;
    record[STORAGE_FORMAT_OFFSET] = STORAGE_FORMAT;
    record[STORAGE_SEQUENCE_OFFSET] = (uint8_t)(sequence >> 8U);
    record[STORAGE_SEQUENCE_OFFSET + 1U] = (uint8_t)sequence;
    memcpy(&record[STORAGE_SENSOR_OFFSET], config->sensor_rom,
           sizeof(config->sensor_rom));
    record[STORAGE_PROFILE_OFFSET] = config->operating_profile;
    memcpy(&record[STORAGE_SETTINGS_OFFSET],
           config->application_settings,
           sizeof(config->application_settings));
    crc = boilerStorageCrc(record, STORAGE_CRC_OFFSET);
    record[STORAGE_CRC_OFFSET] = (uint8_t)(crc >> 8U);
    record[STORAGE_CRC_OFFSET + 1U] = (uint8_t)crc;
    record[STORAGE_MARKER_OFFSET] = STORAGE_COMMIT_MARKER;
}
/*----------------------------------------------------------------------------*/

static void boilerStorageDecode(const uint8_t *record, SBoilerConfig_t *config){

    memcpy(
        config->sensor_rom,
        &record[STORAGE_SENSOR_OFFSET],
        sizeof(config->sensor_rom)
    );
    config->operating_profile = record[STORAGE_PROFILE_OFFSET];
    memcpy(
        config->application_settings,
        &record[STORAGE_SETTINGS_OFFSET],
        sizeof(config->application_settings)
    );
}
/********************** Application Programming Interface *********************/

EResult_t storageInit(void) {
    uint8_t records[STORAGE_SLOT_COUNT][STORAGE_RECORD_SIZE];
    bool valid[STORAGE_SLOT_COUNT];
    uint8_t selected_slot;
    uint8_t slot;
    EResult_t result;

    has_config = false;
    storage_initialized = false;
    if (!boilerStorageLayoutValid()) {
        return eResultOutOfRange;
    }
    for (slot = 0U; slot < STORAGE_SLOT_COUNT; slot++) {
        result = boilerStorageReadRecord(slot, records[slot]);
        if (result != eResultOk) {
            return result;
        }
        valid[slot] = boilerStorageRecordValid(records[slot]);
    }
    storage_initialized = true;

    if (valid[0] && valid[1]) {
        selected_slot = boilerStorageSequenceNewer(
            boilerStorageReadSequence(records[1]),
            boilerStorageReadSequence(records[0])) ? 1U : 0U;
    }
    else if (valid[1]) {
        selected_slot = 1U;
    }
    else if (valid[0]) {
        selected_slot = 0U;
    }
    else {
        return eResultNoConfig;
    }

    boilerStorageDecode(records[selected_slot], &current_config);
    current_sequence = boilerStorageReadSequence(records[selected_slot]);
    active_slot = selected_slot;
    has_config = true;
    return eResultOk;
}

/*----------------------------------------------------------------------------*/

EResult_t storageGet(SBoilerConfig_t *config) {
    if (config == NULL) {
        return eResultInvalidArgument;
    }
    if (!has_config) {
        return eResultNoConfig;
    }

    *config = current_config;
    return eResultOk;
}

/*----------------------------------------------------------------------------*/

EResult_t storageSave(const SBoilerConfig_t *config) {
    uint8_t record[STORAGE_RECORD_SIZE];
    uint8_t verify[STORAGE_RECORD_SIZE];
    uint8_t target_slot;
    uint8_t marker;
    uint16_t next_sequence;
    EResult_t result;

    if (config == NULL) {
        return eResultInvalidArgument;
    }

    if (!storage_initialized) {
        result = storageInit();
        if ((result != eResultOk) && (result != eResultNoConfig)) {
            return result;
        }
    }
    if (!boilerStorageLayoutValid()) {
        return eResultOutOfRange;
    }

    target_slot = has_config ? (uint8_t)(active_slot ^ 1U) : 0U;
    next_sequence = has_config ? (uint16_t)(current_sequence + 1U) : 0U;
    boilerStorageEncode(config, next_sequence, record);
    marker = STORAGE_EMPTY_MARKER;
    result = bspEepromUpdate(
        boilerStorageGetAddress(target_slot) + STORAGE_MARKER_OFFSET,
        &marker, sizeof(marker));
    if (result != eResultOk) {
        return result;
    }
    result = boilerStorageWriteRecord(target_slot, record);
    if (result != eResultOk) {
        return result;
    }
    result = boilerStorageReadRecord(target_slot, verify);
    if (result != eResultOk) {
        return result;
    }

    if ((memcmp(record, verify, STORAGE_MARKER_OFFSET) != 0) ||
        (boilerStorageCrc(verify, STORAGE_CRC_OFFSET) !=
         (uint16_t)(((uint16_t)verify[STORAGE_CRC_OFFSET] << 8U) |
                    verify[STORAGE_CRC_OFFSET + 1U]))) {
        return eResultVerifyFailed;
    }

    marker = STORAGE_COMMIT_MARKER;
    result = bspEepromUpdate(
        boilerStorageGetAddress(target_slot) + STORAGE_MARKER_OFFSET,
        &marker, sizeof(marker));
    if (result != eResultOk) {
        return result;
    }
    result = boilerStorageReadRecord(target_slot, verify);
    if (result != eResultOk) {
        return result;
    }
    if (!boilerStorageRecordValid(verify)) {
        return eResultVerifyFailed;
    }

    boilerStorageDecode(verify, &current_config);
    current_sequence = next_sequence;
    active_slot = target_slot;
    has_config = true;
    return eResultOk;
}
/******************************************************************************/
