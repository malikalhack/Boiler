/**
 * @file    config_test.c
 * @version 0.1.4
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "boiler_config.h"
#include "config.h"
#include "sensors_fake.h"
/****************************** Module variables ******************************/

static SBoilerConfig_t stored;
static EResult_t init_result;
static EResult_t get_result;
static unsigned int storage_calls;

/********************** Application Programming Interface *********************/

EResult_t storageInit(void) {
    storage_calls++;
    return init_result;
}
/*----------------------------------------------------------------------------*/

EResult_t storageGet(SBoilerConfig_t *config) {
    storage_calls++;
    if (get_result == eResultOk) {
        *config = stored;
    }
    return get_result;
}

/****************************** Private functions *****************************/

static SBoilerConfig_t fixture_config(void) {
    SBoilerConfig_t config = {0};
    SSensorRom_t rom;
    uint8_t index;

    config.operating_profile = eProfileTwo;
    for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
        assert(sensors_fake_get_rom(index, &rom) == eResultOk);
        memcpy(config.sensor_rom[index], rom.bytes, SENSOR_ROM_SIZE);
    }
    return config;
}
/*----------------------------------------------------------------------------*/

static void test_validation(void) {
    SBoilerConfig_t valid = fixture_config();
    SBoilerConfig_t changed;
    uint8_t index;

    assert(config_validate(NULL) == eResultInvalidArgument);
    assert(config_validate(&valid) == eResultOk);
    changed = valid;
    changed.operating_profile = 0U;
    assert(config_validate(&changed) == eResultNoConfig);
    changed.operating_profile = (uint8_t)-1;
    assert(config_validate(&changed) == eResultNoConfig);
    for (index = 0U; index < SENSOR_ROM_SIZE; index++) {
        changed = valid;
        changed.sensor_rom[0][index] ^= 1U;
        assert(config_validate(&changed) == eResultNoConfig);
        changed = valid;
        changed.sensor_rom[1][index] ^= 1U;
        assert(config_validate(&changed) == eResultNoConfig);
    }
    changed = valid;
    memcpy(changed.sensor_rom[1], changed.sensor_rom[0], SENSOR_ROM_SIZE);
    assert(config_validate(&changed) == eResultNoConfig);
    memset(changed.sensor_rom[0], 0, SENSOR_ROM_SIZE);
    assert(config_validate(&changed) == eResultNoConfig);
    memset(changed.sensor_rom[0], 0xFF, SENSOR_ROM_SIZE);
    assert(config_validate(&changed) == eResultNoConfig);
    changed = valid;
    changed.operating_profile = eProfileSingle;
    memset(changed.sensor_rom[1], 0xFF, SENSOR_ROM_SIZE);
    assert(config_validate(&changed) == eResultOk);
}
/*----------------------------------------------------------------------------*/

static void test_loading(void) {
    SBoilerConfig_t output;
    SBoilerConfig_t sentinel;

    memset(&sentinel, 0xA5, sizeof(sentinel));
    output = sentinel;
    assert(config_load(NULL) == eResultInvalidArgument);
    assert(storage_calls == 0U);
#if (CONFIG_USE_TEST == 1)
    assert(config_load(&output) == eResultOk);
    assert(storage_calls == 0U);
    assert(output.operating_profile == CONFIG_TEST_PROFILE);
    assert(config_validate(&output) == eResultOk);
#else
    init_result = eResultReadFailed;
    assert(config_load(&output) == eResultReadFailed);
    assert(storage_calls == 1U);
    assert(memcmp(&output, &sentinel, sizeof(output)) == 0);
    init_result = eResultOk;
    get_result = eResultNoConfig;
    assert(config_load(&output) == eResultNoConfig);
    assert(memcmp(&output, &sentinel, sizeof(output)) == 0);
    get_result = eResultOk;
    stored = (SBoilerConfig_t){0};
    assert(config_load(&output) == eResultNoConfig);
    assert(memcmp(&output, &sentinel, sizeof(output)) == 0);
    stored = fixture_config();
    assert(config_load(&output) == eResultOk);
    assert(memcmp(&output, &stored, sizeof(output)) == 0);
#endif /* CONFIG_USE_TEST */
}
/*----------------------------------------------------------------------------*/

static void check_diagnostic(SBoilerConfig_t config, EError_t expected) {
    EError_t error = eErrorWatchdog;

    assert(config_check_sensors(&config, &error) == eResultOk);
    assert(error == expected);
}
/*----------------------------------------------------------------------------*/

static void test_diagnostics(void) {
    SBoilerConfig_t config = fixture_config();
    SBoilerConfig_t swapped;
    uint8_t mask;
    EError_t error = eErrorWatchdog;

    assert(config_check_sensors(NULL, &error) == eResultInvalidArgument);
    assert(config_check_sensors(&config, NULL) == eResultInvalidArgument);
    assert(error == eErrorWatchdog);
    assert(config_check_sensors(&config, &error) == eResultNotReady);
    assert(error == eErrorWatchdog);
    swapped = (SBoilerConfig_t){0};
    check_diagnostic(swapped, eErrorConfigInvalid);
    sensors_init();
    for (mask = 0U; mask < (1U << SENSOR_FAKE_COUNT); mask++) {
        assert(sensors_fake_set_present(0U, (mask & 1U) != 0U) == eResultOk);
        assert(sensors_fake_set_present(1U, (mask & 2U) != 0U) == eResultOk);
        check_diagnostic(config, (mask & 1U) == 0U ? eErrorSensor1 :
                         (mask & 2U) == 0U ? eErrorSensor2 : eErrorNone);
    }
    swapped = config;
    memcpy(swapped.sensor_rom[0], config.sensor_rom[1], SENSOR_ROM_SIZE);
    memcpy(swapped.sensor_rom[1], config.sensor_rom[0], SENSOR_ROM_SIZE);
    assert(sensors_fake_set_present(0U, false) == eResultOk);
    check_diagnostic(swapped, eErrorSensor2);
    assert(sensors_fake_set_present(0U, true) == eResultOk);
    assert(sensors_fake_set_present(1U, false) == eResultOk);
    check_diagnostic(swapped, eErrorSensor1);
    config.operating_profile = eProfileSingle;
    memset(config.sensor_rom[1], 0, SENSOR_ROM_SIZE);
    check_diagnostic(config, eErrorNone);
    assert(sensors_fake_set_present(1U, true) == eResultOk);
    check_diagnostic(config, eErrorUnknownSensor);
    sensors_init();
    config = fixture_config();
    check_diagnostic(config, eErrorNone);
}
/*----------------------------------------------------------------------------*/

int main(void) {
    test_validation();
    test_loading();
    test_diagnostics();
    puts("Configuration tests passed");
    return 0;
}
/******************************************************************************/
