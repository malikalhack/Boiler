/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    boiler_storage.c
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-10-04
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <string.h>

#include "bsp.h"
#include "boiler_storage.h"

/********************************* Definitions ********************************/
/**
 * @def BOILER_STORAGE_MAGIC_0
 * @brief First magic byte identifying an application record.
 */
#define BOILER_STORAGE_MAGIC_0             (0xB0U)
/**
 * @def BOILER_STORAGE_MAGIC_1
 * @brief Second magic byte identifying an application record.
 */
#define BOILER_STORAGE_MAGIC_1             (0x1EU)
/**
 * @def BOILER_STORAGE_FORMAT
 * @brief Current EEPROM record format version.
 */
#define BOILER_STORAGE_FORMAT              (1U)
/**
 * @def BOILER_STORAGE_COMMIT_MARKER
 * @brief Marker written only after a record has been verified.
 */
#define BOILER_STORAGE_COMMIT_MARKER       (0xA5U)
/**
 * @def BOILER_STORAGE_EMPTY_MARKER
 * @brief Marker value used to invalidate the target slot before writing.
 */
#define BOILER_STORAGE_EMPTY_MARKER        (0x00U)
/**
 * @def BOILER_STORAGE_SLOT_COUNT
 * @brief Number of EEPROM configuration slots.
 */
#define BOILER_STORAGE_SLOT_COUNT          (2U)
/**
 * @def BOILER_STORAGE_EEPROM_START
 * @brief First EEPROM address reserved for application configuration.
 */
#define BOILER_STORAGE_EEPROM_START        (0x20U)
/**
 * @def BOILER_STORAGE_SEQUENCE_HALF
 * @brief Half-range used to compare wrapping 16-bit sequence numbers.
 */
#define BOILER_STORAGE_SEQUENCE_HALF       (0x8000U)
/**
 * @def BOILER_STORAGE_MAGIC_0_OFFSET
 * @brief Record offset of the first magic byte.
 */
#define BOILER_STORAGE_MAGIC_0_OFFSET      (0U)
/**
 * @def BOILER_STORAGE_MAGIC_1_OFFSET
 * @brief Record offset of the second magic byte.
 */
#define BOILER_STORAGE_MAGIC_1_OFFSET      (1U)
/**
 * @def BOILER_STORAGE_FORMAT_OFFSET
 * @brief Record offset of the format version.
 */
#define BOILER_STORAGE_FORMAT_OFFSET       (2U)
/**
 * @def BOILER_STORAGE_SEQUENCE_OFFSET
 * @brief Record offset of the two-byte sequence number.
 */
#define BOILER_STORAGE_SEQUENCE_OFFSET     (3U)
/**
 * @def BOILER_STORAGE_SENSOR_OFFSET
 * @brief Record offset of the first sensor ROM code.
 */
#define BOILER_STORAGE_SENSOR_OFFSET       (5U)
/**
 * @def BOILER_STORAGE_PROFILE_OFFSET
 * @brief Record offset of the operating profile.
 */
#define BOILER_STORAGE_PROFILE_OFFSET      (21U)
/**
 * @def BOILER_STORAGE_SETTINGS_OFFSET
 * @brief Record offset of the application settings payload.
 */
#define BOILER_STORAGE_SETTINGS_OFFSET     (22U)
/**
 * @def BOILER_STORAGE_CRC_OFFSET
 * @brief Record offset of the CRC-16 field.
 */
#define BOILER_STORAGE_CRC_OFFSET \
    (BOILER_STORAGE_SETTINGS_OFFSET + BOILER_CONFIG_SETTINGS_SIZE)
/**
 * @def BOILER_STORAGE_MARKER_OFFSET
 * @brief Record offset of the final commit marker.
 */
#define BOILER_STORAGE_MARKER_OFFSET \
    (BOILER_STORAGE_CRC_OFFSET + 2U)
/**
 * @def BOILER_STORAGE_RECORD_SIZE
 * @brief Total size of one EEPROM configuration record in bytes.
 */
#define BOILER_STORAGE_RECORD_SIZE \
    (BOILER_STORAGE_MARKER_OFFSET + 1U)

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
 * @returns true if the EEPROM read succeeds; otherwise false.
 */
static bool boilerStorageReadRecord(uint8_t slot, uint8_t *record);

/**
 * @brief Write record bytes up to, but not including, the commit marker.
 * @param[in] slot Configuration slot index.
 * @param[in] record Encoded record to write.
 * @returns true if the EEPROM update succeeds; otherwise false.
 */
static bool boilerStorageWriteRecord(uint8_t slot, const uint8_t *record);

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

    sequence = (uint16_t)record[BOILER_STORAGE_SEQUENCE_OFFSET] << 8U;
    sequence |= record[BOILER_STORAGE_SEQUENCE_OFFSET + 1U];
    return sequence;
}
/*----------------------------------------------------------------------------*/

static uint16_t boilerStorageGetAddress(uint8_t slot) {
    uint16_t address;

    address = BOILER_STORAGE_EEPROM_START +
              ((uint16_t)slot * BOILER_STORAGE_RECORD_SIZE);
    return address;
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageLayoutValid(void) {
    uint32_t required_size;

    required_size = BOILER_STORAGE_EEPROM_START +
                    (BOILER_STORAGE_SLOT_COUNT *
                     BOILER_STORAGE_RECORD_SIZE);
    return required_size <= bspEepromSize();
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageRecordValid(const uint8_t *record) {
    uint16_t stored_crc;
    uint16_t calculated_crc;

    stored_crc = (uint16_t)record[BOILER_STORAGE_CRC_OFFSET] << 8U;
    stored_crc |= record[BOILER_STORAGE_CRC_OFFSET + 1U];
    calculated_crc = boilerStorageCrc(record, BOILER_STORAGE_CRC_OFFSET);

    return (record[BOILER_STORAGE_MAGIC_0_OFFSET] ==
                BOILER_STORAGE_MAGIC_0) &&
           (record[BOILER_STORAGE_MAGIC_1_OFFSET] ==
                BOILER_STORAGE_MAGIC_1) &&
           (record[BOILER_STORAGE_FORMAT_OFFSET] == BOILER_STORAGE_FORMAT) &&
           (record[BOILER_STORAGE_MARKER_OFFSET] ==
                BOILER_STORAGE_COMMIT_MARKER) &&
           (stored_crc == calculated_crc);
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageSequenceNewer(uint16_t candidate, uint16_t reference) {
    uint16_t difference;

    difference = (uint16_t)(candidate - reference);
    return (candidate != reference) &&
           (difference < BOILER_STORAGE_SEQUENCE_HALF);
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageReadRecord(uint8_t slot, uint8_t *record) {
    return bspEepromRead(
        boilerStorageGetAddress(slot),
        record,
        BOILER_STORAGE_RECORD_SIZE
    );
}
/*----------------------------------------------------------------------------*/

static bool boilerStorageWriteRecord(uint8_t slot, const uint8_t *record) {
    return bspEepromUpdate(
        boilerStorageGetAddress(slot),
        record,
        BOILER_STORAGE_MARKER_OFFSET
    );
}
/*----------------------------------------------------------------------------*/

static void boilerStorageEncode(
    const SBoilerConfig_t *config,
    uint16_t sequence,
    uint8_t *record
) {
    uint16_t crc;

    record[BOILER_STORAGE_MAGIC_0_OFFSET] = BOILER_STORAGE_MAGIC_0;
    record[BOILER_STORAGE_MAGIC_1_OFFSET] = BOILER_STORAGE_MAGIC_1;
    record[BOILER_STORAGE_FORMAT_OFFSET] = BOILER_STORAGE_FORMAT;
    record[BOILER_STORAGE_SEQUENCE_OFFSET] = (uint8_t)(sequence >> 8U);
    record[BOILER_STORAGE_SEQUENCE_OFFSET + 1U] = (uint8_t)sequence;
    memcpy(&record[BOILER_STORAGE_SENSOR_OFFSET], config->sensor_rom,
           sizeof(config->sensor_rom));
    record[BOILER_STORAGE_PROFILE_OFFSET] = config->operating_profile;
    memcpy(&record[BOILER_STORAGE_SETTINGS_OFFSET],
           config->application_settings,
           sizeof(config->application_settings));
    crc = boilerStorageCrc(record, BOILER_STORAGE_CRC_OFFSET);
    record[BOILER_STORAGE_CRC_OFFSET] = (uint8_t)(crc >> 8U);
    record[BOILER_STORAGE_CRC_OFFSET + 1U] = (uint8_t)crc;
    record[BOILER_STORAGE_MARKER_OFFSET] = BOILER_STORAGE_COMMIT_MARKER;
}
/*----------------------------------------------------------------------------*/

static void boilerStorageDecode(const uint8_t *record, SBoilerConfig_t *config){

    memcpy(
        config->sensor_rom,
        &record[BOILER_STORAGE_SENSOR_OFFSET],
        sizeof(config->sensor_rom)
    );
    config->operating_profile = record[BOILER_STORAGE_PROFILE_OFFSET];
    memcpy(
        config->application_settings,
        &record[BOILER_STORAGE_SETTINGS_OFFSET],
        sizeof(config->application_settings)
    );
}
/********************** Application Programming Interface *********************/

bool boilerStorageInit(void) {
    uint8_t records[BOILER_STORAGE_SLOT_COUNT][BOILER_STORAGE_RECORD_SIZE];
    bool valid[BOILER_STORAGE_SLOT_COUNT];
    uint8_t selected_slot;

    has_config = false;
    storage_initialized = true;
    if (!boilerStorageLayoutValid()) {
        return false;
    }
    if (!boilerStorageReadRecord(0U, records[0]) ||
        !boilerStorageReadRecord(1U, records[1])) {
        return false;
    }
    valid[0] = boilerStorageRecordValid(records[0]);
    valid[1] = boilerStorageRecordValid(records[1]);

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
        return false;
    }

    boilerStorageDecode(records[selected_slot], &current_config);
    current_sequence = boilerStorageReadSequence(records[selected_slot]);
    active_slot = selected_slot;
    has_config = true;
    return true;
}

/*----------------------------------------------------------------------------*/

bool boilerStorageGet(SBoilerConfig_t *config) {
    if ((config == NULL) || !has_config) {
        return false;
    }

    *config = current_config;
    return true;
}

/*----------------------------------------------------------------------------*/

bool boilerStorageSave(const SBoilerConfig_t *config) {
    uint8_t record[BOILER_STORAGE_RECORD_SIZE];
    uint8_t verify[BOILER_STORAGE_RECORD_SIZE];
    uint8_t target_slot;
    uint8_t marker;
    uint16_t next_sequence;

    if (config == NULL) {
        return false;
    }

    if (!storage_initialized) {
        (void)boilerStorageInit();
    }
    if (!boilerStorageLayoutValid()) {
        return false;
    }

    target_slot = has_config ? (uint8_t)(active_slot ^ 1U) : 0U;
    next_sequence = has_config ? (uint16_t)(current_sequence + 1U) : 0U;
    boilerStorageEncode(config, next_sequence, record);
    marker = BOILER_STORAGE_EMPTY_MARKER;
    if (!bspEepromUpdate(
            boilerStorageGetAddress(target_slot) +
                BOILER_STORAGE_MARKER_OFFSET,
            &marker, sizeof(marker)) ||
        !boilerStorageWriteRecord(target_slot, record) ||
        !boilerStorageReadRecord(target_slot, verify)) {
        return false;
    }

    if ((memcmp(record, verify, BOILER_STORAGE_MARKER_OFFSET) != 0) ||
        (boilerStorageCrc(verify, BOILER_STORAGE_CRC_OFFSET) !=
         (uint16_t)(((uint16_t)verify[BOILER_STORAGE_CRC_OFFSET] << 8U) |
                    verify[BOILER_STORAGE_CRC_OFFSET + 1U]))) {
        return false;
    }

    marker = BOILER_STORAGE_COMMIT_MARKER;
    if (!bspEepromUpdate(
            boilerStorageGetAddress(target_slot) +
                BOILER_STORAGE_MARKER_OFFSET,
            &marker, sizeof(marker)) ||
        !boilerStorageReadRecord(target_slot, verify) ||
        !boilerStorageRecordValid(verify)) {
        return false;
    }

    boilerStorageDecode(verify, &current_config);
    current_sequence = next_sequence;
    active_slot = target_slot;
    has_config = true;
    return true;
}
/******************************************************************************/
