/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    onewire_test.c
 * @version 0.1.9
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "crc8.h"
#include "onewire.h"
#include "task_priorities.h"

/**
 * @def main
 * @brief Rename firmware startup for execution with the emulated board.
 */
#define main firmware_main
#include "../src/main.c"
#undef main

/********************************* Definitions ********************************/

/**
 * @def TEST_DEVICES
 * @brief Fixture count used to test supported and excess bus populations.
 */
#define TEST_DEVICES (3U)

/**
 * @def TEST_EEPROM_SIZE
 * @brief ATmega16A EEPROM capacity used by the board emulator.
 */
#define TEST_EEPROM_SIZE (512U)

/**
 * @enum EBusMode
 * @brief Expected ROM-command or function stage of the simulated wire bus.
 */
typedef enum EBusMode {
    eBusRom,        /**< Receive a ROM command. */
    eBusSearch,     /**< Resolve search triplets. */
    eBusMatch,      /**< Receive a complete ROM address. */
    eBusFunction,   /**< Receive the addressed sensor command. */
    eBusPower,      /**< Supply the external-power status bit. */
    eBusConvert,    /**< Supply conversion-completion status. */
    eBusScratch,    /**< Supply scratchpad bytes. */
    eBusWriteSetup  /**< Receive volatile alarms and configuration. */
} EBusMode_t;

/****************************** Module variables ******************************/

static SSensorRom_t fixtures[TEST_DEVICES];
static int16_t values[TEST_DEVICES];
static uint8_t resolutions[TEST_DEVICES];
static uint8_t alarms[TEST_DEVICES][2];
static uint8_t setup_bytes[3];
static uint8_t setup_index;
static unsigned int setup_writes;
static bool ignore_setup;
static bool corrupt_alarm;
static bool corrupt_setup_crc;
static bool downgrade_after_convert;
static uint8_t presence;
static uint8_t parasites;
static uint8_t bad_crc;
static bool stuck_low;
static bool wire_initialized;
static bool invalid_scratch;
static bool lost_search;
static uint16_t conversion_duration;
static uint16_t conversion_started;
static unsigned int conversions;
static unsigned int scratch_reads;
static unsigned int resets;
static unsigned int slots;
static unsigned int matches;
static EBusMode_t bus_mode;
static uint8_t candidates;
static uint8_t selected_devices;
static uint8_t search_position;
static uint8_t search_read;
static uint8_t input_byte;
static uint8_t input_bits;
static uint8_t match_index;
static SSensorRom_t matched_rom;
static uint8_t output_bit;
static uint8_t output[9];
static volatile AcroTick_t tick;
static uint8_t eeprom_data[TEST_EEPROM_SIZE];
static unsigned int writes;
static uint8_t raw_buttons;
static bool interrupts_enabled;
static bool buzzer;
static uint8_t frame_slots[DISPLAY_SLOTS];
static uint8_t registered;
static bool app_running;
static jmp_buf scheduler_exit;

/****************************** Private functions *****************************/

/** @brief Restore powered, distinct DS18B20 fixtures and clear injected faults. */
static void reset_bus(void) {
    uint8_t index;

    for (index = 0U; index < TEST_DEVICES; index++) {
        fixtures[index] = (SSensorRom_t){{0x28U, 0x81U, 0U, 0U,
                                        0xA5U, 0U, 0U, 0U}};
        fixtures[index].bytes[2] = index + 1U;
        fixtures[index].bytes[7] = crc8(fixtures[index].bytes, 7U);
        values[index] = (20 + index) * SENSOR_TEMP_SCALE;
        resolutions[index] = SENSOR_CONFIG_12BIT;
        alarms[index][0] = 75U;
        alarms[index][1] = 70U;
    }
    presence = 1U;
    parasites = 0U;
    bad_crc = 0U;
    stuck_low = false;
    wire_initialized = false;
    invalid_scratch = false;
    lost_search = false;
    conversion_duration = SENSOR_CONVERSION_TICKS;
    conversions = 0U;
    scratch_reads = 0U;
    matches = 0U;
    slots = 0U;
    resets = 0U;
    setup_writes = 0U;
    ignore_setup = false;
    corrupt_alarm = false;
    corrupt_setup_crc = false;
    downgrade_after_convert = false;
    tick = 0U;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Consume a complete command, ROM or setup byte from the driver.
 * @param[in] value Received wire byte.
 * @note Unsupported commands, including Copy Scratchpad, fail the test.
 */
static void receive_byte(uint8_t value) {
    uint8_t index;
    uint16_t raw;

    switch (bus_mode) {
        case eBusRom:
            if (value == 0xF0U) {
                bus_mode = eBusSearch;
                candidates = presence;
                search_position = 0U;
                search_read = 0U;
            }
            else if (value == 0x55U) {
                bus_mode = eBusMatch;
                match_index = 0U;
            }
            else {
                assert(value == 0xCCU);
                bus_mode = eBusFunction;
            }
            break;
        case eBusMatch:
            matched_rom.bytes[match_index++] = value;
            if (match_index == SENSOR_ROM_SIZE) {
                matches++;
                selected_devices = 0U;
                for (index = 0U; index < TEST_DEVICES; index++) {
                    if (memcmp(matched_rom.bytes, fixtures[index].bytes,
                               SENSOR_ROM_SIZE) == 0) {
                        selected_devices |= presence & (1U << index);
                    }
                }
                bus_mode = eBusFunction;
            }
            break;
        case eBusFunction:
            if (value == 0xB4U) {
                assert(selected_devices != 0U);
                bus_mode = eBusPower;
            }
            else if (value == 0x44U) {
                conversions++;
                conversion_started = tick;
                bus_mode = eBusConvert;
                if (downgrade_after_convert) {
                    resolutions[0] = 0x3FU;
                }
            }
            else if (value == 0x4EU) {
                setup_index = 0U;
                bus_mode = eBusWriteSetup;
            }
            else {
                assert(value == 0xBEU);
                scratch_reads++;
                bus_mode = eBusScratch;
                output_bit = 0U;
                memset(output, 0xFF, sizeof(output));
                for (index = 0U; index < TEST_DEVICES; index++) {
                    if ((selected_devices & presence & 1U << index) != 0U) {
                        raw = conversions == 0U ?
                            85U * SENSOR_TEMP_SCALE : (uint16_t)values[index];
                        output[0] = raw & 0xFFU;
                        output[1] = raw >> 8U;
                        output[2] = alarms[index][0];
                        output[3] = alarms[index][1];
                        output[4] = resolutions[index];
                        output[5] = 0xFFU;
                        output[6] = 0x0CU;
                        output[7] = 0x10U;
                        output[8] = crc8(output, 8U);
                        if ((bad_crc & 1U << index) != 0U) {
                            output[8] ^= 1U;
                        }
                        else if (corrupt_setup_crc && setup_writes != 0U) {
                            output[8] ^= 1U;
                        }
                    }
                }
                if (invalid_scratch) {
                    memset(output, 0U, sizeof(output));
                }
            }
            break;
        case eBusWriteSetup:
            setup_bytes[setup_index++] = value;
            if (setup_index == sizeof(setup_bytes)) {
                setup_writes++;
                for (index = 0U; index < TEST_DEVICES; index++) {
                    if (
                        !ignore_setup &&
                        (selected_devices & presence & 1U << index) != 0U
                    ) {
                        alarms[index][0] = setup_bytes[0] ^
                                          (corrupt_alarm ? 1U : 0U);
                        alarms[index][1] = setup_bytes[1];
                        resolutions[index] = setup_bytes[2];
                    }
                }
                bus_mode = eBusFunction;
            }
            break;
        default: assert(false); break;
    }
}
/*----------------------------------------------------------------------------*/

/** @brief Advance one backend tick and enforce the per-call bus-work limit. */
static void poll_backend(void) {
    unsigned int previous_slots = slots;
    unsigned int previous_resets = resets;

    sensors_update(tick);
    assert(slots - previous_slots <= 8U);
    assert(resets - previous_resets <= 1U);
    assert(resets == previous_resets || slots == previous_slots);
    tick++;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Poll until the first explicit acquisition result is available.
 * @returns Successful or failed snapshot result.
 * @note A bounded wait fails the test if the driver stalls.
 */
static EResult_t await_snapshot(void) {
    SSensorRom_t roms[SENSOR_CAPACITY];
    uint8_t count;
    uint16_t limit;
    EResult_t result = eResultBusy;

    for (limit = 0U; limit < 2000U && result == eResultBusy; limit++) {
        poll_backend();
        result = sensors_search(roms, SENSOR_CAPACITY, &count);
    }
    assert(result != eResultBusy);
    return result;
}
/*----------------------------------------------------------------------------*/

/** @brief Verify conversion wait boundaries, timeout and bus-loss handling. */
static void test_conversion_boundaries(void) {
    SSensorRom_t roms[SENSOR_CAPACITY];
    uint8_t count = 0xA5U;
    unsigned int previous_slots;
    unsigned int previous_resets;
    uint16_t limit;

    reset_bus();
    sensors_init();
    for (limit = 0U; limit < 200U && conversions == 0U; limit++) {
        poll_backend();
    }
    assert(conversions == 1U);
    tick = conversion_started + SENSOR_CONVERSION_TICKS - 1U;
    previous_slots = slots;
    previous_resets = resets;
    sensors_update(tick);
    assert(slots == previous_slots && resets == previous_resets);
    assert(sensors_search(roms, 2U, &count) == eResultBusy);
    assert(count == 0xA5U);
    tick++;
    sensors_update(tick);
    assert(slots == previous_slots + 1U && resets == previous_resets);
    sensors_update(tick);
    assert(resets == previous_resets + 1U);
    assert(await_snapshot() == eResultOk);

    conversion_duration = SENSOR_TIMEOUT_TICKS + 1U;
    sensors_refresh();
    previous_slots = conversions;
    for (limit = 0U; limit < 200U && conversions == previous_slots; limit++) {
        poll_backend();
    }
    assert(conversions == previous_slots + 1U);
    tick = conversion_started + SENSOR_TIMEOUT_TICKS - 1U;
    sensors_update(tick);
    assert(sensors_search(roms, 2U, &count) == eResultBusy);
    tick++;
    sensors_update(tick);
    assert(sensors_search(roms, 2U, &count) == eResultTimeout);

    reset_bus();
    sensors_init();
    while (conversions == 0U) {
        poll_backend();
    }
    presence = 0U;
    assert(await_snapshot() == eResultBus);
}
/*----------------------------------------------------------------------------*/

/** @brief Exercise discovery, addressing, invalid data and all raw readings. */
static void test_backend(void) {
    static const uint8_t known_scratchpad[] = {
        0x50U, 0x05U, 0x4BU, 0x46U, 0x7FU, 0xFFU, 0x0CU, 0x10U, 0x1CU
    };
    SSensorRom_t roms[SENSOR_CAPACITY];
    SSensorRom_t before[SENSOR_CAPACITY];
    uint8_t count = 0xA5U;
    int16_t temperature = 1234;
    uint8_t mask;
    uint8_t index;
    int16_t raw;
    EError_t error;
    SBoilerConfig_t config = {0};

    reset_bus();
    assert(crc8(known_scratchpad, sizeof(known_scratchpad)) == 0U);
    memset(roms, 0xA5U, sizeof(roms));
    memcpy(before, roms, sizeof(roms));
    assert(sensors_search(NULL, 2U, &count) == eResultInvalidArgument);
    assert(sensors_search(roms, 0U, &count) == eResultInvalidArgument);
    assert(sensors_search(roms, 2U, NULL) == eResultInvalidArgument);
    assert(sensors_read(fixtures[0], NULL) == eResultInvalidArgument);
    assert(sensors_search(roms, 2U, &count) == eResultNotReady);
    assert(sensors_read(fixtures[0], &temperature) == eResultNotReady);
    assert(count == 0xA5U && temperature == 1234);
    assert(memcmp(roms, before, sizeof(roms)) == 0);
    assert(!wire_initialized && resets == 0U && slots == 0U);
    sensors_init();
    assert(sensors_search(roms, 2U, &count) == eResultBusy);
    assert(sensors_read(fixtures[0], &temperature) == eResultBusy);
    assert(await_snapshot() == eResultOk);
    assert(sensors_read(fixtures[0], &temperature) == eResultOk);
    assert(temperature == values[0]);
    assert(conversions == 1U && scratch_reads == 2U && matches == 3U);
    assert(setup_writes == 0U);
    config.operating_profile = eProfileSingle;
    memcpy(config.sensor_rom[0], fixtures[0].bytes, SENSOR_ROM_SIZE);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorNone);

    for (mask = 0U; mask < 4U; mask++) {
        presence = mask;
        sensors_refresh();
        assert(await_snapshot() == eResultOk);
        assert(sensors_search(roms, 2U, &count) == eResultOk);
        assert(count == ((mask & 1U) != 0U) + ((mask & 2U) != 0U));
        for (index = 0U; index < 2U; index++) {
            temperature = 1234;
            assert(sensors_read(fixtures[index], &temperature) ==
                   ((mask & (1U << index)) != 0U ?
                    eResultOk : eResultNotFound));
            assert(temperature == ((mask & (1U << index)) != 0U ?
                                   values[index] : 1234));
        }
        assert(config_check_sensors(&config, &error) == eResultOk);
        assert(error == ((mask & 1U) == 0U ? eErrorSensor1 :
                         mask == 1U ? eErrorNone : eErrorUnknownSensor));
    }
    presence = 3U;
    sensors_refresh();
    assert(await_snapshot() == eResultOk);
    config.operating_profile = eProfileTwo;
    memcpy(config.sensor_rom[0], fixtures[1].bytes, SENSOR_ROM_SIZE);
    memcpy(config.sensor_rom[1], fixtures[0].bytes, SENSOR_ROM_SIZE);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorNone);
    memcpy(before, roms, sizeof(before));
    count = 0xA5U;
    assert(sensors_search(roms, 1U, &count) == eResultOutOfRange);
    assert(count == 0xA5U && memcmp(before, roms, sizeof(before)) == 0);
    assert(sensors_search(NULL, 2U, &count) == eResultInvalidArgument);
    assert(sensors_search(roms, 0U, &count) == eResultInvalidArgument);
    assert(sensors_search(roms, 2U, NULL) == eResultInvalidArgument);
    assert(sensors_read(fixtures[0], NULL) == eResultInvalidArgument);

    presence = 7U;
    sensors_refresh();
    assert(await_snapshot() == eResultOutOfRange);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorDeviceCount);
    presence = 1U;
    fixtures[0].bytes[7] ^= 1U;
    sensors_refresh();
    assert(await_snapshot() == eResultCrc);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorCrc);
    fixtures[0].bytes[7] ^= 1U;
    bad_crc = 1U;
    sensors_refresh();
    assert(await_snapshot() == eResultCrc);
    temperature = 1234;
    assert(sensors_read(fixtures[0], &temperature) == eResultCrc);
    assert(temperature == 1234);
    bad_crc = 0U;
    parasites = 1U;
    sensors_refresh();
    assert(await_snapshot() == eResultPower);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorSoft1Wire);
    parasites = 0U;
    stuck_low = true;
    sensors_refresh();
    assert(await_snapshot() == eResultBus);
    stuck_low = false;
    lost_search = true;
    sensors_refresh();
    assert(await_snapshot() == eResultBus);
    lost_search = false;
    conversion_duration = SENSOR_TIMEOUT_TICKS + 1U;
    sensors_refresh();
    assert(await_snapshot() == eResultTimeout);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorSoft1Wire);
    conversion_duration = SENSOR_CONVERSION_TICKS;
    invalid_scratch = true;
    sensors_refresh();
    assert(await_snapshot() == eResultBus);
    invalid_scratch = false;
    values[0] = 126 * SENSOR_TEMP_SCALE;
    sensors_refresh();
    assert(await_snapshot() == eResultBus);
    for (index = 0U; index < 4U; index++) {
        resolutions[0] = 0x1FU | (index << 5U);
        values[0] = -161;
        sensors_refresh();
        assert(await_snapshot() == eResultOk);
        assert(sensors_read(fixtures[0], &raw) == eResultOk);
        assert(resolutions[0] == SENSOR_CONFIG_12BIT);
        assert(raw == values[0]);
    }
    resolutions[0] = 0x7FU;
    for (raw = -55 * SENSOR_TEMP_SCALE; raw <= 125 * SENSOR_TEMP_SCALE; raw++) {
        values[0] = raw;
        sensors_refresh();
        assert(await_snapshot() == eResultOk);
        assert(sensors_read(fixtures[0], &temperature) == eResultOk);
        assert(temperature == raw);
    }
    fixtures[1].bytes[0] = 0x10U;
    fixtures[1].bytes[7] = crc8(fixtures[1].bytes, 7U);
    presence = 2U;
    sensors_refresh();
    assert(await_snapshot() == eResultOk);
    assert(sensors_search(roms, 2U, &count) == eResultOk && count == 1U);
    assert(sensors_read(fixtures[1], &temperature) == eResultNotFound);
    config.operating_profile = eProfileSingle;
    memcpy(config.sensor_rom[0], fixtures[0].bytes, SENSOR_ROM_SIZE);
    presence = 3U;
    sensors_refresh();
    assert(await_snapshot() == eResultOk);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorUnknownSensor);
    reset_bus();
    tick = UINT16_MAX - 100U;
    sensors_init();
    assert(await_snapshot() == eResultOk);
    assert(sensors_read(fixtures[0], &temperature) == eResultOk);
    /* A new cycle keeps the old verified frame until its new result arrives. */
    bad_crc = 1U;
    for (index = 0U; index < 100U; index++) {
        poll_backend();
    }
    assert(sensors_read(fixtures[0], &temperature) == eResultOk);
    for (raw = 0; raw < 1500; raw++) {
        poll_backend();
    }
    assert(sensors_read(fixtures[0], &temperature) == eResultCrc);
}
/*----------------------------------------------------------------------------*/

/** @brief Verify resolution upgrade, alarm preservation and rejected readback. */
static void test_resolution_upgrade(void) {
    int16_t temperature = 1234;
    uint8_t old_alarms[TEST_DEVICES][2];
    SBoilerConfig_t config = {0};
    EError_t error;

    reset_bus();
    presence = 3U;
    resolutions[0] = 0x3FU;
    resolutions[1] = 0x5FU;
    values[0] = 401;
    values[1] = 509;
    alarms[0][0] = 0x7CU;
    alarms[0][1] = 0xDCU;
    alarms[1][0] = 0x81U;
    alarms[1][1] = 0x23U;
    memcpy(old_alarms, alarms, sizeof(alarms));
    sensors_init();
    assert(await_snapshot() == eResultOk);
    assert(setup_writes == 2U);
    assert(resolutions[0] == SENSOR_CONFIG_12BIT &&
           resolutions[1] == SENSOR_CONFIG_12BIT);
    assert(memcmp(old_alarms, alarms, sizeof(alarms)) == 0);
    assert(sensors_read(fixtures[0], &temperature) == eResultOk);
    assert(temperature == values[0]);
    assert(sensors_read(fixtures[1], &temperature) == eResultOk);
    assert(temperature == values[1]);
    sensors_refresh();
    assert(await_snapshot() == eResultOk);
    assert(setup_writes == 2U);

    reset_bus();
    resolutions[0] = 0x3FU;
    ignore_setup = true;
    sensors_init();
    assert(await_snapshot() == eResultVerifyFailed);
    assert(setup_writes == 1U && conversions == 0U);
    temperature = 1234;
    assert(sensors_read(fixtures[0], &temperature) == eResultVerifyFailed);
    assert(temperature == 1234);
    config.operating_profile = eProfileSingle;
    memcpy(config.sensor_rom[0], fixtures[0].bytes, SENSOR_ROM_SIZE);
    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == eErrorSoft1Wire);
    ignore_setup = false;
    sensors_refresh();
    assert(await_snapshot() == eResultOk);
    assert(setup_writes == 2U && conversions == 1U);

    reset_bus();
    resolutions[0] = 0x1FU;
    corrupt_alarm = true;
    sensors_init();
    assert(await_snapshot() == eResultVerifyFailed);
    assert(setup_writes == 1U && conversions == 0U);

    reset_bus();
    resolutions[0] = 0x5FU;
    corrupt_setup_crc = true;
    sensors_init();
    assert(await_snapshot() == eResultCrc);
    assert(setup_writes == 1U && conversions == 0U);

    reset_bus();
    downgrade_after_convert = true;
    sensors_init();
    assert(await_snapshot() == eResultVerifyFailed);
    assert(conversions == 1U);
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Execute real firmware startup with the emulated board.
 * @param[in] buttons Raw input mask present before reset.
 */
static void boot(uint8_t buttons) {
    raw_buttons = buttons;
    app_running = true;
    if (setjmp(scheduler_exit) == 0) {
        firmware_main();
        assert(false);
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Run display, feedback, button, sensor and service tasks in tick order.
 * @param[in] count Number of simulated system ticks.
 */
static void advance(uint16_t count) {
    while (count-- != 0U) {
        tick++;
        display_scan(NULL);
        feedback_update(NULL);
        buttons_poll(NULL);
        check_sensors(NULL);
        service_update(NULL);
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Deliver one independently debounced press and release.
 * @param[in] button Button to press.
 */
static void press(EButton_t button) {
    raw_buttons = button;
    advance(BUTTON_DEBOUNCE_TICKS + 1U);
    raw_buttons = 0U;
    advance(BUTTON_DEBOUNCE_TICKS + 1U);
}
/*----------------------------------------------------------------------------*/

/** @brief Capture a complete frame after two display scan cycles. */
static void capture_frame(void) {
    advance(DISPLAY_SLOTS * 2U);
}
/*----------------------------------------------------------------------------*/

/** @brief Exercise real assignment, EEPROM handoff, recovery and service entry. */
static void test_firmware(void) {
    SBoilerConfig_t saved;
    unsigned int previous_writes;

    reset_bus();
    memset(eeprom_data, 0xFF, sizeof(eeprom_data));
    writes = 0U;
    boot(0U);
    assert(active_error == eErrorConfigInvalid && !config_loaded);
    capture_frame();
    assert(frame_slots[0] == eGlyphE && frame_slots[3] == eGlyph1);
    assert(frame_slots[4] == eLedMode3);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuProfile);
    press(eButtonRight);
    assert(presence == 1U);
    capture_frame();
    assert(frame_slots[0] == eGlyphP && frame_slots[3] == eGlyph1);
    assert(frame_slots[4] == eLedMode4);
    assert((frame_slots[0] & eGlyphPoint) == 0U);
    press(eButtonEnter);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuMeasure1);
    advance(1100U);
    assert(menu_get_state() == eMenuCheck);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuVerify);
    advance(1100U);
    assert(menu_get_state() == eMenuSave && writes == 0U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuPreSave);
    advance(1200U);
    assert(menu_get_state() == eMenuIdle && config_loaded);
    assert(active_error == eErrorNone && !buzzer);
    assert(storageGet(&saved) == eResultOk);
    assert(saved.operating_profile == eProfileSingle);
    assert(memcmp(saved.sensor_rom[0], fixtures[0].bytes, SENSOR_ROM_SIZE) == 0);
    capture_frame();
    assert(frame_slots[0] == eGlyphBlank && frame_slots[1] == eGlyph2);
    assert(frame_slots[2] == (eGlyph0 | eGlyphPoint) &&
           frame_slots[3] == eGlyph0);
    assert(frame_slots[4] == (eLedChannel1 | eLedMode3));
    previous_writes = writes;
    boot(0U);
    assert(config_loaded && active_error == eErrorNone);
    capture_frame();
    assert(frame_slots[0] == eGlyphDash);
    assert(frame_slots[4] == eLedMode3);
    advance(1100U);
    assert(active_error == eErrorNone);
    presence = 0U;
    advance(1600U);
    assert(active_error == eErrorSensor1);
    presence = 1U;
    advance(1600U);
    assert(active_error == eErrorNone && !buzzer);
    bad_crc = 1U;
    advance(1600U);
    assert(active_error == eErrorCrc);
    bad_crc = 0U;
    stuck_low = true;
    advance(1600U);
    assert(active_error == eErrorSoft1Wire);
    stuck_low = false;
    advance(1600U);
    assert(active_error == eErrorNone);
    presence = 3U;
    advance(1800U);
    assert(active_error == eErrorUnknownSensor);
    boot(eButtonLeft | eButtonRight);
    advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS + 2U);
    assert(menu_get_state() == eMenuProfile);
    raw_buttons = 0U;
    advance(BUTTON_DEBOUNCE_TICKS + 1U);
    presence = 1U;
    press(eButtonLeft);
    advance(1100U);
    assert(menu_get_state() == eMenuIdle && active_error == eErrorNone);
    assert(writes == previous_writes);
    /* A failed check is retryable without changing EEPROM. */
    assert(menu_open(active_error) == eResultOk);
    press(eButtonEnter);
    bad_crc = 1U;
    press(eButtonEnter);
    advance(1100U);
    assert(menu_get_state() == eMenuFailure);
    assert(menu_get_result() == eResultCrc);
    bad_crc = 0U;
    press(eButtonEnter);
    advance(1100U);
    assert(menu_get_state() == eMenuCheck);
    press(eButtonLeft);
    advance(1100U);
    assert(active_error == eErrorNone && writes == previous_writes);
    fixtures[1].bytes[0] = 0x10U;
    fixtures[1].bytes[7] = crc8(fixtures[1].bytes, 7U);
    assert(menu_open(active_error) == eResultOk);
    press(eButtonEnter);
    presence = 2U;
    press(eButtonEnter);
    advance(1100U);
    assert(menu_get_state() == eMenuFailure);
    assert(menu_get_result() == eResultNoConfig);
    capture_frame();
    assert(frame_slots[0] == eGlyphE && frame_slots[3] == eGlyph4);
    assert(writes == previous_writes);
}

/********************** Application Programming Interface *********************/

void onewire_init(void) {
    wire_initialized = true;
    bus_mode = eBusRom;
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_reset(void) {
    assert(wire_initialized);
    resets++;
    input_byte = 0U;
    input_bits = 0U;
    bus_mode = eBusRom;
    selected_devices = presence;
    if (stuck_low) {
        return eResultBus;
    }
    return presence == 0U ? eResultNotFound : eResultOk;
}
/*----------------------------------------------------------------------------*/

void onewire_write_bit(bool value) {
    uint8_t index;
    bool bit;

    assert(wire_initialized);
    slots++;
    if (bus_mode == eBusSearch) {
        assert(search_read == 2U);
        for (index = 0U; index < TEST_DEVICES; index++) {
            bit = (fixtures[index].bytes[search_position / 8U] &
                   (1U << (search_position % 8U))) != 0U;
            if (bit != value) {
                candidates &= (uint8_t)~(1U << index);
            }
        }
        search_read = 0U;
        search_position++;
    }
    else {
        if (value) {
            input_byte |= 1U << input_bits;
        }
        if (++input_bits == 8U) {
            receive_byte(input_byte);
            input_byte = 0U;
            input_bits = 0U;
        }
    }
}
/*----------------------------------------------------------------------------*/

bool onewire_read_bit(void) {
    uint8_t index;
    bool value = true;
    bool bit;

    assert(wire_initialized);
    slots++;
    switch (bus_mode) {
        case eBusSearch:
            assert(search_position < 64U && search_read < 2U);
            for (index = 0U; index < TEST_DEVICES; index++) {
                if (!lost_search && (candidates & 1U << index) != 0U) {
                    bit = (fixtures[index].bytes[search_position / 8U] &
                           (1U << (search_position % 8U))) != 0U;
                    value &= search_read == 0U ? bit : !bit;
                }
            }
            search_read++;
            return value;
        case eBusPower:
            return (parasites & selected_devices & presence) == 0U;
        case eBusConvert:
            return (uint16_t)(tick - conversion_started) >= conversion_duration;
        case eBusScratch:
            assert(output_bit < sizeof(output) * 8U);
            value = (output[output_bit / 8U] &
                     (1U << (output_bit % 8U))) != 0U;
            output_bit++;
            return value;
        default: assert(false); return false;
    }
}
/*----------------------------------------------------------------------------*/

void bspInit(void) {
    wire_initialized = false;
    interrupts_enabled = false;
}
/*----------------------------------------------------------------------------*/

void bspStart(void) {
    interrupts_enabled = true;
}
/*----------------------------------------------------------------------------*/

volatile AcroTick_t *bspGetTick(void) {
    return &tick;
}
/*----------------------------------------------------------------------------*/

AcroTick_t bsp_get_time(void) {
    return tick;
}
/*----------------------------------------------------------------------------*/

uint8_t bsp_get_buttons(void) {
    return raw_buttons;
}
/*----------------------------------------------------------------------------*/

void bsp_buzzer_set(bool enabled) {
    buzzer = enabled;
}
/*----------------------------------------------------------------------------*/

void bspHeaterOff(EBspHeater_t heater) {
    assert(heater == eBspHeater1 || heater == eBspHeater2);
}
/*----------------------------------------------------------------------------*/

void bsp_display_write(uint8_t data, uint8_t mask) {
    uint8_t index;

    for (index = 0U; index < DISPLAY_SLOTS; index++) {
        if (mask == 1U << index) {
            frame_slots[index] = data;
        }
    }
}
/*----------------------------------------------------------------------------*/

uint16_t bspEepromSize(void) {
    return TEST_EEPROM_SIZE;
}
/*----------------------------------------------------------------------------*/

bool bsp_eeprom_ready(void) {
    return true;
}
/*----------------------------------------------------------------------------*/

EResult_t bspEepromRead(uint16_t address, uint8_t *data, uint16_t size) {
    assert((uint32_t)address + size <= TEST_EEPROM_SIZE);
    memcpy(data, &eeprom_data[address], size);
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

EResult_t bspEepromUpdate(uint16_t address, const uint8_t *data, uint16_t size) {
    assert((uint32_t)address + size <= TEST_EEPROM_SIZE);
    memcpy(&eeprom_data[address], data, size);
    writes++;
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroInit(volatile AcroTick_t const *source) {
    assert(source == &tick && !interrupts_enabled);
    registered = 0U;
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroAddTask(
    uint8_t descriptor, AcroTask_t *task, AcroParam_t params,
    uint8_t mode, AcroTick_t period, uint8_t priority
) {
    assert(wire_initialized);
    assert(!interrupts_enabled && descriptor >= eTaskDisplay &&
           descriptor <= eTaskMenu && task != NULL);
    assert(params == NULL && mode == ePeriodic && period == 1U);
    assert(priority == task_priorities[descriptor]);
    assert((registered & (1U << descriptor)) == 0U);
    registered |= 1U << descriptor;
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroRun(void) {
    assert(interrupts_enabled && buzzer);
    assert(registered == 0x3EU);
    assert(app_running);
    longjmp(scheduler_exit, 1);
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Run the real-bus backend and firmware integration test suites.
 * @returns Zero after all assertions pass.
 */
int main(void) {
    test_backend();
    test_conversion_boundaries();
    test_resolution_upgrade();
    test_firmware();
    puts("Software 1-Wire protocol, DS18B20 and real firmware tests passed");
    return 0;
}
/******************************************************************************/
