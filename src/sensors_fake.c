/**
 * @file    sensors_fake.c
 * @version 0.1.4
 * @authors Anton Chernov
 * @date    2026-11-05
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "boiler_config.h"

#if (SENSORS_USE_FAKE == 1)
#include <string.h>

#include "sensors_fake.h"
/********************************* Definitions ********************************/

/**
 * @def SENSOR_MIN_TEMP
 * @brief DS18B20 lower measurement limit in raw units.
 */
#define SENSOR_MIN_TEMP (-55 * SENSOR_TEMP_SCALE)

/**
 * @def SENSOR_MAX_TEMP
 * @brief DS18B20 upper measurement limit in raw units.
 */
#define SENSOR_MAX_TEMP (125 * SENSOR_TEMP_SCALE)

/****************************** Module variables ******************************/

static const SSensorRom_t fixture_roms[SENSOR_FAKE_COUNT] = {
    {{0x28U, 0x01U, 0U, 0U, 0U, 0U, 0U, 0x29U}},
    {{0x28U, 0x02U, 0U, 0U, 0U, 0U, 0U, 0x70U}}
};
static const int16_t initial_temperatures[SENSOR_FAKE_COUNT] = {
    25 * SENSOR_TEMP_SCALE, 30 * SENSOR_TEMP_SCALE
};
static bool initialized;
static bool connected[SENSOR_FAKE_COUNT];
static int16_t temperatures[SENSOR_FAKE_COUNT];

/********************** Application Programming Interface *********************/

void sensors_init(void) {
    uint8_t index;

    for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
        connected[index] = true;
        temperatures[index] = initial_temperatures[index];
    }
    initialized = true;
}
/*----------------------------------------------------------------------------*/

EResult_t sensors_search(SSensorRom_t *roms, uint8_t capacity, uint8_t *count) {
    uint8_t index;
    uint8_t found = 0U;

    if ((roms == NULL) || (count == NULL) || (capacity == 0U)) {
        return eResultInvalidArgument;
    }
    if (!initialized) {
        return eResultNotReady;
    }
    for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
        if (connected[index]) {
            found++;
        }
    }
    if (found > capacity) {
        return eResultOutOfRange;
    }
    found = 0U;
    for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
        if (connected[index]) {
            roms[found] = fixture_roms[index];
            found++;
        }
    }
    *count = found;
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

EResult_t sensors_read(SSensorRom_t rom, int16_t *temperature) {
    uint8_t index;
    EResult_t ret_val = eResultNotFound;

    if (temperature == NULL) {
        return eResultInvalidArgument;
    }
    if (!initialized) {
        return eResultNotReady;
    }
    for (index = 0U; index < SENSOR_FAKE_COUNT; index++) {
        if (connected[index] &&
            (memcmp(rom.bytes, fixture_roms[index].bytes,
                    SENSOR_ROM_SIZE) == 0)) {
            *temperature = temperatures[index];
            ret_val = eResultOk;
            break;
        }
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

EResult_t sensors_fake_get_rom(uint8_t index, SSensorRom_t *rom) {
    if ((index >= SENSOR_FAKE_COUNT) || (rom == NULL)) {
        return eResultInvalidArgument;
    }
    *rom = fixture_roms[index];
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

EResult_t sensors_fake_set_present(uint8_t index, bool present) {
    if (index >= SENSOR_FAKE_COUNT) {
        return eResultInvalidArgument;
    }
    if (!initialized) {
        return eResultNotReady;
    }
    connected[index] = present;
    return eResultOk;
}
/*----------------------------------------------------------------------------*/

EResult_t sensors_fake_set_temperature(uint8_t index, int16_t temperature) {
    if (index >= SENSOR_FAKE_COUNT) {
        return eResultInvalidArgument;
    }
    if (!initialized) {
        return eResultNotReady;
    }
    if ((temperature < SENSOR_MIN_TEMP) || (temperature > SENSOR_MAX_TEMP)) {
        return eResultOutOfRange;
    }
    temperatures[index] = temperature;
    return eResultOk;
}
/******************************************************************************/
#endif /* SENSORS_USE_FAKE */
