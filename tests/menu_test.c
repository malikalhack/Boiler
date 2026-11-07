/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    menu_test.c
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include "task_priorities.h"
#include "sensor_stub.h"

/**
 * @def main
 * @brief Run the real firmware startup with mocked board/scheduler.
 */
#define main firmware_main
#include "../src/main.c"
#undef main
/********************************* Definitions ********************************/

/**
 * @def TEST_EEPROM_SIZE
 * @brief ATmega16A EEPROM capacity.
 */
#define TEST_EEPROM_SIZE        (512U)

/**
 * @def TEST_WRITE_TICKS
 * @brief Simulated EEPROM byte write duration.
 */
#define TEST_WRITE_TICKS        (3U)

/**
 * @def TEST_SAVE_LIMIT
 * @brief Upper bound for a complete cooperative transaction.
 */
#define TEST_SAVE_LIMIT         (1000U)

/**
 * @def TEST_COMMIT_MARKER
 * @brief Persistent record commit marker used for fault injection.
 */
#define TEST_COMMIT_MARKER      (0xA5U)

/**
 * @def TEST_SAVE_BYTE_WRITES
 * @brief Invalidate, forty payload bytes and final commit marker.
 */
#define TEST_SAVE_BYTE_WRITES   (42U)

/****************************** Module variables ******************************/

static uint8_t eeprom_data[TEST_EEPROM_SIZE];
static volatile AcroTick_t tick;
static uint8_t raw_buttons;
static bool interrupts_enabled;
static bool buzzer;
static uint8_t segments;
static uint8_t selected;
static uint8_t registered;
static uint8_t heaters_off;
static uint8_t write_ticks;
static uint16_t pending_address;
static uint8_t pending_byte;
static unsigned int writes;
static unsigned int failed_write;
static bool corrupt_byte;
static bool corrupt_commit;
static bool read_failure;
static jmp_buf scheduler_exit;

/********************** Application Programming Interface *********************/

void bspInit(void) {
    interrupts_enabled = false;
    heaters_off = 0U;
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
    heaters_off |= 1U << heater;
}
/*----------------------------------------------------------------------------*/

void bsp_display_write(uint8_t data, uint8_t mask) {
    segments = data;
    selected = mask;
}
/*----------------------------------------------------------------------------*/
bool bsp_eeprom_ready(void) {
    return write_ticks == 0U;
}
/*----------------------------------------------------------------------------*/

uint16_t bspEepromSize(void) {
    return TEST_EEPROM_SIZE;
}
/*----------------------------------------------------------------------------*/

EResult_t bspEepromRead(uint16_t address, uint8_t *data, uint16_t size) {
    assert(bsp_eeprom_ready());
    assert((uint32_t)address + size <= TEST_EEPROM_SIZE);
    if (read_failure) {
        return eResultReadFailed;
    }
    memcpy(data, &eeprom_data[address], size);
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

EResult_t bspEepromUpdate(uint16_t address, const uint8_t *data, uint16_t size) {
    assert(bsp_eeprom_ready());
    assert(size == 1U);
    assert(address < TEST_EEPROM_SIZE);
    writes++;
    if (writes == failed_write) {
        return eResultWriteFailed;
    }
    pending_address = address;
    pending_byte = *data;
    if (corrupt_byte) {
        pending_byte ^= 1U;
    }
    else if (corrupt_commit && pending_byte == TEST_COMMIT_MARKER) {
        pending_byte ^= 1U;
    }
    write_ticks = TEST_WRITE_TICKS;
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroInit(volatile AcroTick_t const *source) {
    assert(source == &tick);
    assert(!interrupts_enabled);
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroAddTask(
    uint8_t descriptor, AcroTask_t *task, AcroParam_t params,
    uint8_t mode, AcroTick_t period, uint8_t priority
) {
    assert(!interrupts_enabled);
    assert(params == NULL && mode == ePeriodic);
    assert(descriptor >= eTaskDisplay && descriptor <= eTaskMenu);
    assert(priority == task_priorities[descriptor]);
    assert((registered & (1U << descriptor)) == 0U);
    registered |= 1U << descriptor;
    assert(period == 1U);
    if (descriptor == eTaskMenu) {
        assert(task == service_update);
    }
    if (descriptor == eTaskButtons) {
        assert(task == buttons_poll);
    }
    return eAcroOk;
}
/*----------------------------------------------------------------------------*/

EAcroStatus_t acroRun(void) {
    assert(interrupts_enabled);
    assert(registered == ((1U << eTaskDisplay) | (1U << eTaskFeedback) |
                          (1U << eTaskButtons) | (1U << eTaskSensors) |
                          (1U << eTaskMenu)));
    longjmp(scheduler_exit, 1);
}

/****************************** Private functions *****************************/

static void advance(uint16_t ticks) {
    uint16_t index;
    unsigned int previous;

    for (index = 0U; index < ticks; index++) {
        tick++;
        if (write_ticks != 0U) {
            write_ticks--;
            if (write_ticks == 0U) {
                eeprom_data[pending_address] = pending_byte;
            }
        }
        previous = writes;
        feedback_update(NULL);
        buttons_poll(NULL);
        check_sensors(NULL);
        service_update(NULL);
        assert(writes <= previous + 1U);
    }
}
/*----------------------------------------------------------------------------*/

static void press(EButton_t button) {
    raw_buttons = button;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
    raw_buttons = 0U;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
}
/*----------------------------------------------------------------------------*/

static void boot_with_buttons(uint8_t buttons, AcroTick_t initial_tick) {
    tick = initial_tick;
    raw_buttons = buttons;
    registered = 0U;
    stub_presence = STUB_ALL_PRESENT;
    if (setjmp(scheduler_exit) == 0) {
        firmware_main();
        assert(false);
    }
    check_sensors(NULL);
}
/*----------------------------------------------------------------------------*/

static void boot_board(void) {
    boot_with_buttons(0U, 0U);
}
/*----------------------------------------------------------------------------*/

static void reset_board(void) {
    assert(write_ticks == 0U);
    memset(eeprom_data, 0xFF, sizeof(eeprom_data));
    writes = 0U;
    failed_write = 0U;
    corrupt_byte = false;
    corrupt_commit = false;
    read_failure = false;
    boot_board();
    assert(!config_loaded);
    assert(active_error == eErrorConfigInvalid);
    assert(menu_get_state() == eMenuIdle);
}
/*----------------------------------------------------------------------------*/

static void check_frame(SDisplayFrame_t expected, uint8_t points) {
    uint8_t position;
    uint8_t pattern;

    for (position = 0U; position < DISPLAY_SLOTS; position++) {
        display_scan(NULL);
        assert(selected == (1U << position));
        pattern = expected.slots[position];
        if (
            position < DISPLAY_DIGITS &&
            (points & 1U << position) != 0U
        ) {
            pattern |= eGlyphPoint;
        }
        assert(segments == pattern);
    }
}
/*----------------------------------------------------------------------------*/

static void select_presence(uint8_t mask) {
    assert(mask <= STUB_ALL_PRESENT);
    stub_presence = mask;
    check_sensors(NULL);
}
/*----------------------------------------------------------------------------*/

static void check_error(EError_t error) {
    static const uint8_t glyphs[] = {
        eGlyph0, eGlyph1, eGlyph2, eGlyph3, eGlyph4,
        eGlyph5, eGlyph6, eGlyph7, eGlyph8, eGlyph9
    };
    SDisplayFrame_t frame = {{eGlyphE, eGlyph0, eGlyph0, 0U, 0U}};

    frame.slots[3] = glyphs[error];
    frame.slots[DISPLAY_DIGITS] = eLedMode4;
    check_frame(frame, 0U);
}
/*----------------------------------------------------------------------------*/

static void check_prompt(SDisplayFrame_t frame) {
    frame.slots[DISPLAY_DIGITS] = eLedMode4;
    check_frame(frame, 0U);
}
/*----------------------------------------------------------------------------*/

static void prepare_save(bool two) {
    press(eButtonEnter);
    assert(menu_get_state() == eMenuProfile);
    check_prompt((SDisplayFrame_t){{eGlyphP, eGlyphBlank,
                                   eGlyphBlank, eGlyph1, 0U}});
    assert(heaters_off == 3U);
    if (two) {
        press(eButtonUp);
        press(eButtonUp);
        check_prompt((SDisplayFrame_t){{eGlyphP, eGlyphBlank,
                                       eGlyphBlank, eGlyph2, 0U}});
    }
    select_presence(1U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuSensor1);
    check_prompt((SDisplayFrame_t){{eGlyph5, eGlyph1,
                                   eGlyphBlank, eGlyphBlank, 0U}});
    press(eButtonEnter);
    assert(menu_get_state() == (two ? eMenuSensor2 : eMenuCheck));
    if (two) {
        check_prompt((SDisplayFrame_t){{eGlyph5, eGlyph2,
                                       eGlyphBlank, eGlyphBlank, 0U}});
        select_presence(2U);
        press(eButtonEnter);
        assert(menu_get_state() == eMenuCheck);
        select_presence(3U);
    }
    check_prompt((SDisplayFrame_t){{eGlyphC, eGlyph0, eGlyphN, eGlyphF, 0U}});
    press(eButtonEnter);
    assert(menu_get_state() == eMenuSave);
    check_prompt((SDisplayFrame_t){{eGlyph5, eGlyphA, eGlyphV, eGlyphE, 0U}});
}
/*----------------------------------------------------------------------------*/

static void finish_save(void) {
    uint16_t count;

    for (count = 0U; count < TEST_SAVE_LIMIT &&
         menu_get_state() == eMenuSaving; count++) {
        advance(1U);
    }
    assert(menu_get_state() != eMenuSaving);
    assert(write_ticks == 0U);
}
/*----------------------------------------------------------------------------*/

static void test_success(bool two) {
    SBoilerConfig_t config;
    SSensorRom_t rom;
    SDisplayFrame_t temperature = {
        {eGlyphBlank, eGlyph2, eGlyph5 | eGlyphPoint, eGlyph0,
         eLedChannel1 | eLedMode3}
    };

    reset_board();
    prepare_save(two);
    assert(writes == 0U);
    assert(storageGet(&config) == eResultNoConfig);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuSaving);
    assert(menu_get_result() == eResultBusy);
    assert(storageInit() == eResultBusy);
    assert(storageSave(&config) == eResultBusy);
    assert(storage_save_begin(&config) == eResultBusy);
    assert(!config_loaded);
    press(eButtonLeft);
    finish_save();
    assert(menu_get_state() == eMenuIdle);
    assert(menu_get_result() == eResultOk);
    assert(storageGet(&config) == eResultOk);
    assert(config_validate(&config) == eResultOk);
    assert(config.operating_profile == (two ? eProfileTwo : eProfileSingle));
    rom = stub_roms[0];
    assert(memcmp(config.sensor_rom[0], rom.bytes, SENSOR_ROM_SIZE) == 0);
    if (two) {
        rom = stub_roms[1];
        assert(memcmp(config.sensor_rom[1], rom.bytes, SENSOR_ROM_SIZE) == 0);
    }
    assert(config_loaded);
    tick = (tick / FEEDBACK_BLINK_TICKS + 1U) * FEEDBACK_BLINK_TICKS;
    feedback_update(NULL);
    assert(active_error == eErrorNone);
    assert(!menu_is_active());
    assert(!buzzer);
    check_frame(temperature, 0U);
    assert(storageInit() == eResultOk);
    assert(storageGet(&config) == eResultOk);
    assert(config_validate(&config) == eResultOk);
    boot_board();
    assert(config_loaded);
    assert(active_error == (two ? eErrorNone : eErrorUnknownSensor));
    assert(menu_get_state() == eMenuIdle);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuIdle);
}
/*----------------------------------------------------------------------------*/

static void test_cancel_and_measurement_errors(void) {
    SDisplayFrame_t error = {{eGlyphE, eGlyph0, eGlyph0, eGlyph1, eLedMode3}};
    SBoilerConfig_t config;

    reset_board();
    raw_buttons = eButtonEnter;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS - 1U);
    assert(menu_get_state() == eMenuIdle);
    advance(1U);
    assert(menu_get_state() == eMenuProfile);
    advance(BUTTON_HOLD_TICKS + BUTTON_REPEAT_TICKS);
    assert(menu_get_state() == eMenuProfile);
    raw_buttons = 0U;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
    press(eButtonRight);
    assert(menu_get_state() == eMenuProfile);
    assert(stub_presence == STUB_ALL_PRESENT);
    check_prompt((SDisplayFrame_t){{
        eGlyphP, eGlyphBlank, eGlyphBlank, eGlyph1, 0U}});
    advance(FEEDBACK_BLINK_TICKS / 2U);
    check_prompt((SDisplayFrame_t){{
        eGlyphP, eGlyphBlank, eGlyphBlank, eGlyph1, 0U}});
    press(eButtonEnter);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuFailure);
    check_error(eErrorDeviceCount);
    advance(FEEDBACK_BLINK_TICKS / 2U);
    check_frame((SDisplayFrame_t){{
        eGlyphE, eGlyph0, eGlyph0, eGlyph6, 0U}}, 0U);
    advance(FEEDBACK_BLINK_TICKS / 2U);
    check_error(eErrorDeviceCount);
    select_presence(0U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuFailure);
    select_presence(1U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuCheck);
    select_presence(2U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuFailure);
    check_error(eErrorSensor1);
    press(eButtonLeft);
    assert(menu_get_state() == eMenuIdle);
    assert(writes == 0U);
    assert(storageGet(&config) == eResultNoConfig);
    tick += FEEDBACK_BLINK_TICKS;
    feedback_update(NULL);
    check_frame(error, 0U);
    prepare_save(true);
    press(eButtonLeft);
    assert(menu_get_state() == eMenuIdle);
    assert(writes == 0U);
}
/*----------------------------------------------------------------------------*/

static void test_duplicate_and_pre_save_check(void) {
    reset_board();
    press(eButtonEnter);
    press(eButtonUp);
    select_presence(1U);
    press(eButtonEnter);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuSensor2);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuFailure);
    check_error(eErrorConfigInvalid);
    select_presence(2U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuCheck);
    select_presence(3U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuSave);
    select_presence(1U);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuFailure);
    check_error(eErrorSensor2);
    assert(writes == 0U);
    press(eButtonLeft);
}
/*----------------------------------------------------------------------------*/

static void test_enter_chords(void) {
    reset_board();
    raw_buttons = eButtonUp | eButtonEnter;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(menu_get_state() == eMenuIdle);
    raw_buttons = 0U;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuProfile);
    raw_buttons = eButtonRight | eButtonEnter;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
    assert(menu_get_state() == eMenuProfile);
    assert(writes == 0U);
    raw_buttons = 0U;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
    press(eButtonLeft);
    assert(menu_get_state() == eMenuIdle);
}
/*----------------------------------------------------------------------------*/

static void test_save_failure_and_retry(void) {
    SBoilerConfig_t config;
    SBoilerConfig_t actual;
    unsigned int count;

    reset_board();
    prepare_save(false);
    failed_write = writes + 2U;
    press(eButtonEnter);
    assert(menu_get_state() == eMenuFailure);
    assert(menu_get_result() == eResultWriteFailed);
    assert(storageGet(&config) == eResultNoConfig);
    failed_write = 0U;
    press(eButtonEnter);
    finish_save();
    assert(menu_get_state() == eMenuIdle);
    assert(config_loaded);
    assert(storageGet(&config) == eResultOk);

    menu_init(eErrorConfigInvalid);
    stub_presence = 1U;
    config_loaded = false;
    active_error = eErrorConfigInvalid;
    press(eButtonEnter);
    select_presence(2U);
    press(eButtonEnter);
    press(eButtonEnter);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuSave);
    failed_write = writes + 2U;
    press(eButtonEnter);
    assert(menu_get_result() == eResultWriteFailed);
    assert(storageGet(&actual) == eResultOk);
    assert(memcmp(&actual, &config, sizeof(config)) == 0);
    failed_write = 0U;
    press(eButtonLeft);
    assert(storageInit() == eResultOk);
    assert(storageGet(&actual) == eResultOk);
    assert(memcmp(&actual, &config, sizeof(config)) == 0);

    for (count = 0U; count < 2U; count++) {
        reset_board();
        prepare_save(false);
        corrupt_byte = count == 0U;
        corrupt_commit = count == 1U;
        press(eButtonEnter);
        finish_save();
        assert(menu_get_state() == eMenuFailure);
        assert(menu_get_result() == eResultVerifyFailed);
        assert(storageGet(&config) == eResultNoConfig);
        corrupt_byte = false;
        corrupt_commit = false;
        press(eButtonLeft);
    }
}
/*----------------------------------------------------------------------------*/

static void test_each_write_failure(void) {
    SBoilerConfig_t config;
    unsigned int offset;

    for (offset = 1U; offset <= TEST_SAVE_BYTE_WRITES; offset++) {
        reset_board();
        prepare_save(false);
        failed_write = writes + offset;
        press(eButtonEnter);
        finish_save();
        assert(menu_get_state() == eMenuFailure);
        assert(menu_get_result() == eResultWriteFailed);
        assert(storageGet(&config) == eResultNoConfig);
        press(eButtonLeft);
    }
    reset_board();
    prepare_save(false);
    press(eButtonEnter);
    read_failure = true;
    finish_save();
    assert(menu_get_state() == eMenuFailure);
    assert(menu_get_result() == eResultReadFailed);
    read_failure = false;
    press(eButtonEnter);
    finish_save();
    assert(menu_get_state() == eMenuIdle);
    assert(config_loaded);
}
/*----------------------------------------------------------------------------*/

static void test_single_fixture_application(uint8_t presence) {
    uint8_t actual_presence;

    reset_board();
    press(eButtonEnter);
    select_presence(presence);
    press(eButtonEnter);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuCheck);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuSave);
    press(eButtonEnter);
    finish_save();
    assert(menu_get_state() == eMenuIdle);
    assert(config_loaded);
    assert(active_error == eErrorNone);
    assert(!buzzer);
    actual_presence = stub_presence;
    assert(actual_presence == presence);
    stub_temperatures[0] = 25 * SENSOR_TEMP_SCALE;
    stub_temperatures[1] = 30 * SENSOR_TEMP_SCALE;
    check_sensors(NULL);
    check_frame((SDisplayFrame_t){{eGlyphBlank,
                                  presence == 1U ? eGlyph2 : eGlyph3,
                                  (presence == 1U ? eGlyph5 : eGlyph0) |
                                  eGlyphPoint, eGlyph0,
                                  eLedChannel1 | eLedMode3}},
                0U);
    advance(2U * FEEDBACK_BLINK_TICKS);
    assert(active_error == eErrorNone);
    actual_presence = stub_presence;
    assert(actual_presence == presence);
    assert(menu_open(eErrorNone) == eResultOk);
    check_frame((SDisplayFrame_t){{
        eGlyphP, eGlyphBlank, eGlyphBlank, eGlyph1, eLedMode4}}, 0U);
    press(eButtonLeft);
    assert(menu_get_state() == eMenuIdle);
}
/*----------------------------------------------------------------------------*/

static void test_temperature_controls_and_service(void) {
    static const EButton_t buttons[] = {
        eButtonLeft, eButtonRight, eButtonUp, eButtonDown, eButtonEnter
    };
    SBoilerConfig_t before;
    SBoilerConfig_t after;
    uint8_t presence;
    uint8_t index;
    unsigned int previous_writes;
    SDisplayFrame_t temperature = {
        {eGlyphBlank, eGlyph2, eGlyph5 | eGlyphPoint, eGlyph0,
         eLedChannel1 | eLedMode3}
    };

    test_success(true);
    boot_board();
    assert(storageGet(&before) == eResultOk);
    previous_writes = writes;
    stub_temperatures[0] = 25 * SENSOR_TEMP_SCALE;
    stub_temperatures[1] = 30 * SENSOR_TEMP_SCALE;
    check_sensors(NULL);
    check_frame(temperature, 0U);
    stub_temperatures[0] = -55 * SENSOR_TEMP_SCALE;
    check_sensors(NULL);
    check_frame((SDisplayFrame_t){{
        eGlyphDash, eGlyph5, eGlyph5 | eGlyphPoint, eGlyph0,
        eLedChannel1 | eLedMode3}}, 0U);
    stub_temperatures[0] = 125 * SENSOR_TEMP_SCALE;
    check_sensors(NULL);
    check_frame((SDisplayFrame_t){{
        eGlyph1, eGlyph2, eGlyph5 | eGlyphPoint, eGlyph0,
        eLedChannel1 | eLedMode3}}, 0U);
    stub_temperatures[0] = 25 * SENSOR_TEMP_SCALE;
    check_sensors(NULL);
    check_frame(temperature, 0U);
    for (index = 0U; index < sizeof(buttons) / sizeof(buttons[0]); index++) {
        raw_buttons = buttons[index];
        buttons_poll(NULL);
        advance(BUTTON_DEBOUNCE_TICKS);
        raw_buttons = 0U;
        buttons_poll(NULL);
        advance(BUTTON_DEBOUNCE_TICKS);
        assert(menu_get_state() == eMenuIdle);
        presence = stub_presence;
        assert(presence == 3U);
        check_frame(temperature, 0U);
        assert(writes == previous_writes);
    }
    assert(storageGet(&after) == eResultOk);
    assert(memcmp(&before, &after, sizeof(before)) == 0);
    assert(menu_open(active_error) == eResultOk);
    check_prompt((SDisplayFrame_t){
        {eGlyphP, eGlyphBlank, eGlyphBlank, eGlyph1, 0U}});
    advance(1000U);
    check_prompt((SDisplayFrame_t){
        {eGlyphP, eGlyphBlank, eGlyphBlank, eGlyph1, 0U}});
    press(eButtonLeft);
    assert(!menu_is_active());
    temperature = (SDisplayFrame_t){
        {eGlyphBlank, eGlyph2, eGlyph5 | eGlyphPoint, eGlyph0,
         eLedChannel1 | eLedMode3}
    };
    check_frame(temperature, 0U);
    stub_presence &= (uint8_t)~1U;
    check_sensors(NULL);
    check_frame((SDisplayFrame_t){{eGlyphE, eGlyph0, eGlyph0, eGlyph2,
                                  eLedMode3}}, 0U);
    tick += FEEDBACK_BLINK_TICKS / 2U;
    feedback_update(NULL);
    show_status();
    check_frame((SDisplayFrame_t){{
        eGlyphE, eGlyph0, eGlyph0, eGlyph2, 0U}}, 0U);
    tick += FEEDBACK_BLINK_TICKS / 2U;
    feedback_update(NULL);
    show_status();
    check_frame((SDisplayFrame_t){{
        eGlyphE, eGlyph0, eGlyph0, eGlyph2, eLedMode3}}, 0U);
    stub_presence |= 1U;
    check_sensors(NULL);
    check_frame(temperature, 0U);
    assert(!buzzer);
    assert(heaters_off == 3U);
    assert(writes == previous_writes);
}
/*----------------------------------------------------------------------------*/

static void release_buttons(void) {
    raw_buttons = 0U;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS);
}
/*----------------------------------------------------------------------------*/

static void test_automatic_save_completion(void) {
    uint8_t mask;
    uint8_t presence;
    unsigned int previous_writes;

    for (mask = 0U; mask <= 3U; mask++) {
        reset_board();
        prepare_save(false);
        raw_buttons = eButtonEnter;
        buttons_poll(NULL);
        advance(BUTTON_DEBOUNCE_TICKS);
        assert(menu_get_state() == eMenuSaving);
        assert(!config_loaded);
        stub_presence = mask;
        finish_save();
        assert(menu_get_state() == eMenuIdle);
        assert(config_loaded);
        assert(menu_get_result() == eResultOk);
        assert(active_error == ((mask & 1U) == 0U ? eErrorSensor1 :
                                mask == 1U ? eErrorNone : eErrorUnknownSensor));
        presence = stub_presence;
        assert(presence == mask);
        previous_writes = writes;
        advance(BUTTON_HOLD_TICKS + BUTTON_REPEAT_TICKS);
        assert(menu_get_state() == eMenuIdle);
        assert(writes == previous_writes);
        assert(!buzzer);
        release_buttons();
    }
}
/*----------------------------------------------------------------------------*/

static void test_startup_chord(void) {
    SBoilerConfig_t before;
    SBoilerConfig_t after;
    unsigned int previous_writes;

    test_success(true);
    assert(storageGet(&before) == eResultOk);
    previous_writes = writes;
    boot_with_buttons(eButtonLeft | eButtonRight, 0U);
    advance(BUTTON_DEBOUNCE_TICKS - 1U);
    assert(menu_get_state() == eMenuIdle);
    advance(1U);
    assert(startup_holding);
    advance(MENU_STARTUP_HOLD_TICKS - 1U);
    assert(menu_get_state() == eMenuIdle);
    advance(1U);
    assert(menu_get_state() == eMenuProfile);
    assert(heaters_off == 3U);
    advance(MENU_STARTUP_HOLD_TICKS);
    assert(menu_get_state() == eMenuProfile);
    release_buttons();
    select_presence(1U);
    press(eButtonEnter);
    press(eButtonEnter);
    assert(menu_get_state() == eMenuCheck);
    assert(storageGet(&after) == eResultOk);
    assert(memcmp(&before, &after, sizeof(before)) == 0);
    assert(writes == previous_writes);
    press(eButtonLeft);
    assert(menu_get_state() == eMenuIdle);
    assert(storageInit() == eResultOk);
    assert(storageGet(&after) == eResultOk);
    assert(memcmp(&before, &after, sizeof(before)) == 0);
    assert(writes == previous_writes);
    advance(BOILER_TICK_HZ / 10U);
    assert(active_error == eErrorSensor2);

    boot_with_buttons(eButtonLeft | eButtonRight, 0U);
    advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS - 1U);
    release_buttons();
    assert(menu_get_state() == eMenuIdle);
    assert(!startup_pending);

    boot_with_buttons(eButtonLeft | eButtonRight, 0U);
    advance(BUTTON_DEBOUNCE_TICKS - 1U);
    release_buttons();
    assert(menu_get_state() == eMenuIdle);
    assert(!startup_pending);
    raw_buttons = eButtonLeft | eButtonRight;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS);
    assert(menu_get_state() == eMenuIdle);
    release_buttons();

    boot_board();
    raw_buttons = eButtonLeft | eButtonRight;
    buttons_poll(NULL);
    advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS);
    assert(menu_get_state() == eMenuIdle);
    release_buttons();

    boot_with_buttons(eButtonLeft | eButtonRight,
                      (AcroTick_t)(UINT16_MAX - BUTTON_DEBOUNCE_TICKS));
    advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS - 1U);
    assert(menu_get_state() == eMenuIdle);
    advance(1U);
    assert(menu_get_state() == eMenuProfile);
    release_buttons();
    press(eButtonLeft);
}
/*----------------------------------------------------------------------------*/

static void test_startup_input_settling(void) {
    const uint8_t chord = eButtonLeft | eButtonRight;
    const uint8_t all_buttons = eButtonDown | eButtonRight | eButtonUp |
                                eButtonLeft | eButtonEnter;
    uint8_t initial;

    reset_board();
    for (initial = chord; initial <= all_buttons; initial++) {
        if ((initial & chord) != chord) {
            continue;
        }
        boot_with_buttons(initial, 0U);
        advance(BUTTON_DEBOUNCE_TICKS - 1U);
        raw_buttons = chord;
        advance(1U);
        assert(startup_holding);
        advance(MENU_STARTUP_HOLD_TICKS - 1U);
        assert(menu_get_state() == eMenuIdle);
        advance(1U);
        assert(menu_get_state() == eMenuProfile);
        release_buttons();
        press(eButtonLeft);
        assert(writes == 0U);

        if (initial == chord) {
            continue;
        }
        boot_with_buttons(initial, 0U);
        advance(BUTTON_DEBOUNCE_TICKS);
        assert(!startup_pending);
        raw_buttons = chord;
        advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS);
        assert(menu_get_state() == eMenuIdle);
        release_buttons();
        assert(writes == 0U);
    }
}
/*----------------------------------------------------------------------------*/

static void test_startup_chord_from_e004(void) {
    SBoilerConfig_t before;
    SBoilerConfig_t after;
    unsigned int previous_writes;

    test_success(false);
    assert(active_error == eErrorUnknownSensor);
    assert(storageGet(&before) == eResultOk);
    previous_writes = writes;
    boot_with_buttons(eButtonLeft | eButtonRight, 0U);
    assert(active_error == eErrorUnknownSensor);
    advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS);
    assert(menu_get_state() == eMenuProfile);
    release_buttons();
    press(eButtonLeft);
    assert(menu_get_state() == eMenuIdle);
    assert(storageGet(&after) == eResultOk);
    assert(memcmp(&before, &after, sizeof(before)) == 0);
    assert(writes == previous_writes);
    assert(active_error == eErrorUnknownSensor);
}
/*----------------------------------------------------------------------------*/

static void test_startup_chord_from_missing_sensors(void) {
    uint8_t mask;

    test_success(true);
    for (mask = 0U; mask <= 1U; mask++) {
        boot_with_buttons(eButtonLeft | eButtonRight, 0U);
        stub_presence = mask;
        check_sensors(NULL);
        assert(active_error == (mask == 0U ? eErrorSensor1 : eErrorSensor2));
        advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS);
        assert(menu_get_state() == eMenuProfile);
        assert(menu_open(active_error) == eResultBusy);
        release_buttons();
        press(eButtonLeft);
        assert(menu_get_state() == eMenuIdle);
        assert(active_error == (mask == 0U ? eErrorSensor1 : eErrorSensor2));
    }
}
/*----------------------------------------------------------------------------*/

static void test_reassignment_and_monitor_boundary(void) {
    SBoilerConfig_t config;
    AcroTick_t interval = 1U;
    AcroTick_t wait;
    uint8_t presence;
    bool boundary_seen = false;

    for (wait = 0U; wait < interval; wait++) {
        test_success(false);
        boot_with_buttons(eButtonLeft | eButtonRight, 0U);
        advance(BUTTON_DEBOUNCE_TICKS + MENU_STARTUP_HOLD_TICKS);
        release_buttons();
        assert(menu_get_state() == eMenuProfile);
        press(eButtonUp);
        select_presence(2U);
        press(eButtonEnter);
        press(eButtonEnter);
        assert(menu_get_state() == eMenuSensor2);
        select_presence(1U);
        press(eButtonEnter);
        assert(menu_get_state() == eMenuCheck);
        select_presence(3U);
        press(eButtonEnter);
        assert(menu_get_state() == eMenuSave);
        assert(storageGet(&config) == eResultOk);
        assert(config.operating_profile == eProfileSingle);
        advance(wait);
        press(eButtonEnter);
        finish_save();
        boundary_seen |= (tick % interval) == 0U;
        assert(menu_get_state() == eMenuIdle);
        assert(active_error == eErrorNone);
        assert(active_config.operating_profile == eProfileTwo);
        presence = stub_presence;
        assert(presence == 3U);
        advance(interval);
        assert(active_error == eErrorNone);
        boot_board();
        assert(active_error == eErrorNone);
        assert(config_loaded);
    }
    assert(boundary_seen);
}
/*----------------------------------------------------------------------------*/

int main(void) {
    assert(storage_save_poll() == eResultNotReady);
    test_success(false);
    test_success(true);
    test_cancel_and_measurement_errors();
    test_duplicate_and_pre_save_check();
    test_enter_chords();
    test_save_failure_and_retry();
    test_each_write_failure();
    test_single_fixture_application(1U);
    test_single_fixture_application(2U);
    test_temperature_controls_and_service();
    test_automatic_save_completion();
    test_startup_chord();
    test_startup_input_settling();
    test_startup_chord_from_e004();
    test_startup_chord_from_missing_sensors();
    test_reassignment_and_monitor_boundary();
    puts("Assignment menu and cooperative EEPROM tests passed");
    return 0;
}
/******************************************************************************/
