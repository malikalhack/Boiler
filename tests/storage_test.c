/**
 * @file    storage_test.c
 * @version 0.1.1
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "storage.h"
#include "bsp.h"
/********************************* Definitions ********************************/

/**
 * @def TEST_EEPROM_SIZE
 * @brief EEPROM capacity of the ATmega16A in bytes.
 */
#define TEST_EEPROM_SIZE (512U)

/**
 * @def TEST_SAVE_WRITES
 * @brief Marker invalidation, record data, then commit marker writes.
 */
#define TEST_SAVE_WRITES (3U)

/**
 * @def TEST_SAVE_READS
 * @brief Verification reads before and after the commit marker write.
 */
#define TEST_SAVE_READS (2U)

/****************************** Module variables ******************************/

static uint8_t eeprom_data[TEST_EEPROM_SIZE];
static uint16_t capacity = TEST_EEPROM_SIZE;
static bool read_failure;
static bool corrupt_write;
static bool corrupt_commit;
static unsigned int read_count;
static unsigned int failed_read;
static unsigned int update_count;
static unsigned int failed_update;

/********************** Application Programming Interface *********************/

EResult_t bspEepromRead(uint16_t address, uint8_t *data, uint16_t size) {
    EResult_t ret_val;

    read_count++;
    ret_val = eResultInvalidArgument;
    if ((data == NULL) || (size == 0U)) {
        return ret_val;
    }
    if ((uint32_t)address + size > capacity) {
        return eResultOutOfRange;
    }
    ret_val = eResultReadFailed;
    if (!read_failure && (read_count != failed_read)) {
        memcpy(data, &eeprom_data[address], size);
        ret_val = eResultOk;
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

EResult_t bspEepromUpdate(uint16_t address, const uint8_t *data, uint16_t size) {
    EResult_t ret_val;

    update_count++;
    ret_val = eResultInvalidArgument;
    if ((data == NULL) || (size == 0U)) {
        return ret_val;
    }
    if ((uint32_t)address + size > capacity) {
        return eResultOutOfRange;
    }
    ret_val = eResultWriteFailed;
    if (update_count != failed_update) {
        memcpy(&eeprom_data[address], data, size);
        if (corrupt_write && (size > 1U)) {
            eeprom_data[address] ^= 1U;
        }
        if (corrupt_commit && (update_count == TEST_SAVE_WRITES)) {
            eeprom_data[address] ^= 1U;
        }
        ret_val = eResultOk;
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

uint16_t bspEepromSize(void) {
    return capacity;
}

/****************************** Private functions *****************************/

static void check_cached(const SBoilerConfig_t *expected) {
    SBoilerConfig_t actual;

    assert(storageGet(&actual) == eResultOk);
    assert(memcmp(actual.sensor_rom, expected->sensor_rom,
                  sizeof(actual.sensor_rom)) == 0);
    assert(actual.operating_profile == expected->operating_profile);
    assert(memcmp(actual.application_settings, expected->application_settings,
                  sizeof(actual.application_settings)) == 0);
}
/*----------------------------------------------------------------------------*/

static void test_storage(void) {
    SBoilerConfig_t first;
    SBoilerConfig_t next;
    SBoilerConfig_t actual;
    uint8_t saved_eeprom[TEST_EEPROM_SIZE];
    unsigned int failure;

    memset(&first, 0, sizeof(first));
    first.sensor_rom[0][0] = 0x28U;
    first.operating_profile = 1U;
    next = first;
    next.application_settings[0] = 1U;

    assert(storageGet(&actual) == eResultNoConfig);
    assert(storageGet(NULL) == eResultInvalidArgument);
    assert(storageSave(NULL) == eResultInvalidArgument);
    memset(eeprom_data, 0xFF, sizeof(eeprom_data));
    read_failure = true;
    assert(storageSave(&first) == eResultReadFailed);
    assert(update_count == 0U);
    read_failure = false;
    assert(storageSave(&first) == eResultOk);
    check_cached(&first);
    memset(eeprom_data, 0xFF, sizeof(eeprom_data));
    assert(storageInit() == eResultNoConfig);
    assert(storageSave(&first) == eResultOk);
    check_cached(&first);
    assert(storageInit() == eResultOk);
    check_cached(&first);
    memcpy(saved_eeprom, eeprom_data, sizeof(saved_eeprom));

    for (failure = 1U; failure <= TEST_SAVE_WRITES; failure++) {
        memcpy(eeprom_data, saved_eeprom, sizeof(eeprom_data));
        assert(storageInit() == eResultOk);
        update_count = 0U;
        failed_update = failure;
        assert(storageSave(&next) == eResultWriteFailed);
        check_cached(&first);
        failed_update = 0U;
        assert(storageInit() == eResultOk);
        check_cached(&first);
    }

    for (failure = 1U; failure <= TEST_SAVE_READS; failure++) {
        memcpy(eeprom_data, saved_eeprom, sizeof(eeprom_data));
        assert(storageInit() == eResultOk);
        read_count = 0U;
        failed_read = failure;
        assert(storageSave(&next) == eResultReadFailed);
        check_cached(&first);
        failed_read = 0U;
    }
    memcpy(eeprom_data, saved_eeprom, sizeof(eeprom_data));
    assert(storageInit() == eResultOk);

    corrupt_write = true;
    assert(storageSave(&next) == eResultVerifyFailed);
    check_cached(&first);
    corrupt_write = false;

    update_count = 0U;
    corrupt_commit = true;
    assert(storageSave(&next) == eResultVerifyFailed);
    check_cached(&first);
    corrupt_commit = false;

    read_failure = true;
    assert(storageSave(&next) == eResultReadFailed);
    check_cached(&first);
    assert(storageInit() == eResultReadFailed);
    assert(storageGet(&actual) == eResultNoConfig);
    update_count = 0U;
    assert(storageSave(&next) == eResultReadFailed);
    assert(update_count == 0U);
    read_failure = false;

    assert(storageInit() == eResultOk);
    check_cached(&first);
    assert(storageSave(&next) == eResultOk);
    check_cached(&next);
    assert(storageInit() == eResultOk);
    check_cached(&next);

    memset(eeprom_data, 0, sizeof(eeprom_data));
    assert(storageInit() == eResultNoConfig);
    assert(storageGet(&actual) == eResultNoConfig);

    capacity = 1U;
    update_count = 0U;
    assert(storageInit() == eResultOutOfRange);
    assert(storageSave(&next) == eResultOutOfRange);
    assert(update_count == 0U);
}
/*----------------------------------------------------------------------------*/

int main(void) {
    test_storage();
    puts("Storage status tests passed");
    return 0;
}
/******************************************************************************/
