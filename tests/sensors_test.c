/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    sensors_test.c
 * @version 0.1.6
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sensors_fake.h"

/********************************* Definitions ********************************/

/**
 * @def TEST_DALLAS_POLYNOMIAL
 * @brief Reflected Dallas/Maxim CRC-8 polynomial.
 */
#define TEST_DALLAS_POLYNOMIAL  (0x8CU)

/**
 * @def TEST_BITS_PER_BYTE
 * @brief Number of CRC shifts for each byte.
 */
#define TEST_BITS_PER_BYTE      (8U)

/**
 * @def TEST_SENTINEL
 * @brief Unchanged-output marker for failed calls.
 */
#define TEST_SENTINEL           (0xA5U)

/****************************** Private functions *****************************/

static uint8_t rom_crc(SSensorRom_t rom) {
    uint8_t crc = 0U;
    uint8_t byte;
    uint8_t bit;

    for (byte = 0U; byte < SENSOR_ROM_SIZE; byte++) {
        crc ^= rom.bytes[byte];
        for (bit = 0U; bit < TEST_BITS_PER_BYTE; bit++) {
            crc = (crc >> 1U) ^
                ((crc & 1U) != 0U ? TEST_DALLAS_POLYNOMIAL : 0U);
        }
    }
    return crc;
}
/*----------------------------------------------------------------------------*/

static void test_initialization(void) {
    SSensorRom_t rom;
    SSensorRom_t output;
    SSensorRom_t sentinel;
    uint8_t count = TEST_SENTINEL;
    int16_t temperature = TEST_SENTINEL;
    uint8_t presence = TEST_SENTINEL;

    memset(&sentinel, TEST_SENTINEL, sizeof(sentinel));
    output = sentinel;
    assert(sensors_fake_get_rom(0U, &rom) == eResultOk);
    assert(sensors_search(&output, 1U, &count) == eResultNotReady);
    assert(count == TEST_SENTINEL);
    assert(memcmp(&output, &sentinel, sizeof(output)) == 0);
    assert(sensors_read(rom, &temperature) == eResultNotReady);
    assert(temperature == TEST_SENTINEL);
    assert(sensors_fake_set_present(0U, false) == eResultNotReady);
    assert(sensors_fake_set_temperature(0U, 0) == eResultNotReady);
    assert(sensors_fake_get_presence(&presence) == eResultNotReady);
    assert(presence == TEST_SENTINEL);
    assert(sensors_fake_get_presence(NULL) == eResultInvalidArgument);
    assert(sensors_fake_sweep_start(0U) == eResultNotReady);
    assert(sensors_fake_sweep_update(0U) == eResultNotReady);
    sensors_init();
    assert(sensors_fake_sweep_update(0U) == eResultNotReady);
    assert(sensors_fake_get_presence(&presence) == eResultOk);
    assert(presence == (1U << SENSOR_FAKE_COUNT) - 1U);
}
/*----------------------------------------------------------------------------*/

static void test_discovery(void) {
    SSensorRom_t roms[SENSOR_FAKE_COUNT];
    SSensorRom_t fixtures[SENSOR_FAKE_COUNT];
    SSensorRom_t sentinel[SENSOR_FAKE_COUNT];
    uint8_t count;
    uint8_t mask;
    uint8_t index;
    uint8_t expected;
    uint8_t presence;
    int16_t temperature;

    sensors_init();
    for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
        assert(sensors_fake_get_rom(index, &fixtures[index]) == eResultOk);
        assert(fixtures[index].bytes[0] == 0x28U);
        assert(rom_crc(fixtures[index]) == 0U);
    }
    assert(memcmp(&fixtures[0], &fixtures[1], sizeof(fixtures[0])) != 0);
    memset(sentinel, TEST_SENTINEL, sizeof(sentinel));
    memcpy(roms, sentinel, sizeof(roms));
    count = TEST_SENTINEL;
    assert(sensors_search(roms, 1U, &count) == eResultOutOfRange);
    assert(count == TEST_SENTINEL);
    assert(memcmp(roms, sentinel, sizeof(roms)) == 0);
    for (mask = 0U; mask < (1U << SENSOR_FAKE_COUNT); mask++) {
        for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
            assert(sensors_fake_set_present(
                index, (mask & (1U << index)) != 0U) == eResultOk);
        }
        assert(sensors_fake_get_presence(&presence) == eResultOk);
        assert(presence == mask);
        memcpy(roms, sentinel, sizeof(roms));
        assert(sensors_search(roms, SENSOR_FAKE_COUNT, &count) == eResultOk);
        expected = 0U;
        for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
            temperature = TEST_SENTINEL;
            if ((mask & (1U << index)) != 0U) {
                assert(memcmp(
                    &roms[expected], &fixtures[index], sizeof(fixtures[index])
                ) == 0);
                assert(sensors_read(fixtures[index], &temperature) ==
                       eResultOk);
                expected++;
            }
            else {
                assert(sensors_read(fixtures[index], &temperature) ==
                       eResultNotFound);
                assert(temperature == TEST_SENTINEL);
            }
        }
        assert(count == expected);
        if (count < SENSOR_FAKE_COUNT) {
            assert(memcmp(&roms[count], &sentinel[count],
                          (SENSOR_FAKE_COUNT - count) * sizeof(roms[0])) == 0);
        }
    }
    sensors_init();
    assert(sensors_search(roms, SENSOR_FAKE_COUNT, &count) == eResultOk);
    assert(count == SENSOR_FAKE_COUNT);
    assert(sensors_read(fixtures[0], &temperature) == eResultOk);
    assert(temperature == 25 * SENSOR_TEMP_SCALE);
    assert(sensors_read(fixtures[1], &temperature) == eResultOk);
    assert(temperature == 30 * SENSOR_TEMP_SCALE);
}
/*----------------------------------------------------------------------------*/

static void test_temperature_and_rom_addressing(void) {
    SSensorRom_t rom;
    SSensorRom_t changed;
    int16_t temperature;
    int16_t value;
    uint8_t byte;

    sensors_init();
    assert(sensors_fake_get_rom(0U, &rom) == eResultOk);
    for (
        value = -55 * SENSOR_TEMP_SCALE;
        value <= 125 * SENSOR_TEMP_SCALE;
        value++
    ) {
        assert(sensors_fake_set_temperature(0U, value) == eResultOk);
        assert(sensors_read(rom, &temperature) == eResultOk);
        assert(temperature == value);
    }
    assert(sensors_fake_set_temperature(0U, -55 * SENSOR_TEMP_SCALE - 1) ==
           eResultOutOfRange);
    assert(sensors_fake_set_temperature(0U, 125 * SENSOR_TEMP_SCALE + 1) ==
           eResultOutOfRange);
    assert(sensors_read(rom, &temperature) == eResultOk);
    assert(temperature == 125 * SENSOR_TEMP_SCALE);
    for (byte = 0U; byte < SENSOR_ROM_SIZE; byte++) {
        changed = rom;
        changed.bytes[byte] ^= 1U;
        temperature = TEST_SENTINEL;
        assert(sensors_read(changed, &temperature) == eResultNotFound);
        assert(temperature == TEST_SENTINEL);
    }
    assert(sensors_fake_set_present(0U, false) == eResultOk);
    assert(sensors_fake_set_temperature(0U, -1) == eResultOk);
    assert(sensors_fake_set_present(0U, true) == eResultOk);
    assert(sensors_read(rom, &temperature) == eResultOk);
    assert(temperature == -1);
    sensors_init();
    assert(sensors_read(rom, &temperature) == eResultOk);
    assert(temperature == 25 * SENSOR_TEMP_SCALE);
}
/*----------------------------------------------------------------------------*/

static void test_invalid_arguments(void) {
    SSensorRom_t rom;
    SSensorRom_t before;
    uint8_t count = TEST_SENTINEL;
    int16_t temperature = TEST_SENTINEL;

    sensors_init();
    assert(sensors_fake_get_rom(0U, &rom) == eResultOk);
    before = rom;
    assert(sensors_search(NULL, SENSOR_FAKE_COUNT, &count) ==
           eResultInvalidArgument);
    assert(sensors_search(&rom, 1U, NULL) == eResultInvalidArgument);
    assert(sensors_search(&rom, 0U, &count) == eResultInvalidArgument);
    assert(count == TEST_SENTINEL);
    assert(memcmp(&rom, &before, sizeof(rom)) == 0);
    assert(sensors_read(rom, NULL) == eResultInvalidArgument);
    assert(sensors_fake_get_rom(0U, NULL) == eResultInvalidArgument);
    assert(sensors_fake_get_rom(SENSOR_FAKE_COUNT, &rom) ==
           eResultInvalidArgument);
    assert(memcmp(&rom, &before, sizeof(rom)) == 0);
    assert(sensors_fake_set_present(SENSOR_FAKE_COUNT, false) ==
           eResultInvalidArgument);
    assert(sensors_fake_set_temperature(SENSOR_FAKE_COUNT, 0) ==
           eResultInvalidArgument);
    assert(sensors_read(rom, &temperature) == eResultOk);
    assert(temperature == 25 * SENSOR_TEMP_SCALE);
}
/*----------------------------------------------------------------------------*/

static void check_sweep(int16_t tenths) {
    SSensorRom_t rom;
    int16_t raw;
    int16_t expected;
    uint8_t index;
    double exact = tenths * (double)SENSOR_TEMP_SCALE / 10.0;

    expected = (int16_t)(exact + (tenths < 0 ? -0.5 : 0.5));
    for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
        assert(sensors_fake_get_rom(index, &rom) == eResultOk);
        assert(sensors_read(rom, &raw) == eResultOk);
        assert(raw == expected);
        assert((raw * 10 + (raw < 0 ? -8 : 8)) / SENSOR_TEMP_SCALE ==
               tenths);
    }
}
/*----------------------------------------------------------------------------*/

static void test_sweep(void) {
    uint16_t now = UINT16_MAX - 200U;
    uint16_t step;
    uint8_t presence;

    sensors_init();
    assert(sensors_fake_sweep_start(now) == eResultOk);
    check_sweep(-550);
    for (step = 1U; step <= 1801U; step++) {
        now += SENSOR_FAKE_STEP_TICKS - 1U;
        assert(sensors_fake_sweep_update(now) == eResultOk);
        check_sweep((int16_t)step - 551);
        now++;
        assert(sensors_fake_sweep_update(now) == eResultOk);
        check_sweep(step == 1801U ? -550 : (int16_t)step - 550);
    }
    now += 3U * SENSOR_FAKE_STEP_TICKS + 17U;
    assert(sensors_fake_sweep_update(now) == eResultOk);
    check_sweep(-547);
    now += SENSOR_FAKE_STEP_TICKS - 18U;
    assert(sensors_fake_sweep_update(now) == eResultOk);
    check_sweep(-547);
    now++;
    assert(sensors_fake_sweep_update(now) == eResultOk);
    check_sweep(-546);
    assert(sensors_fake_set_present(1U, false) == eResultOk);
    now += 100U * SENSOR_FAKE_STEP_TICKS;
    assert(sensors_fake_sweep_update(now) == eResultOk);
    assert(sensors_fake_get_presence(&presence) == eResultOk);
    assert(presence == 1U);
    assert(sensors_fake_set_present(1U, true) == eResultOk);
    check_sweep(-446);
    assert(sensors_fake_set_present(0U, false) == eResultOk);
    assert(sensors_fake_sweep_start(now) == eResultOk);
    assert(sensors_fake_get_presence(&presence) == eResultOk);
    assert(presence == 2U);
    assert(sensors_fake_set_present(0U, true) == eResultOk);
    check_sweep(-550);
    sensors_init();
    assert(sensors_fake_sweep_update(now) == eResultNotReady);
}
/*----------------------------------------------------------------------------*/

int main(void) {
    test_initialization();
    test_discovery();
    test_temperature_and_rom_addressing();
    test_invalid_arguments();
    test_sweep();
    puts("Fake sensor tests passed");
    return 0;
}
/******************************************************************************/
