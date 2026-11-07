/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    sensor_stub.c
 * @version 0.1.7
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 * @brief Host-only sensor API substitute; never linked into AVR firmware.
 */

/******************************** Included files ******************************/
#include <string.h>

#include "sensor_stub.h"
/****************************** Module variables ******************************/

const SSensorRom_t stub_roms[SENSOR_CAPACITY] = {
    {{0x28U, 0x01U, 0U, 0U, 0U, 0U, 0U, 0x29U}},
    {{0x28U, 0x02U, 0U, 0U, 0U, 0U, 0U, 0x70U}}
};
uint8_t stub_presence = STUB_ALL_PRESENT;
int16_t stub_temperatures[SENSOR_CAPACITY] = {
    25 * SENSOR_TEMP_SCALE, 30 * SENSOR_TEMP_SCALE
};
static EResult_t snapshot_result = eResultNotReady;

/********************** Application Programming Interface *********************/

void sensors_init(void) {
    sensors_refresh();
}
/*----------------------------------------------------------------------------*/

void sensors_refresh(void) {
    snapshot_result = eResultBusy;
}
/*----------------------------------------------------------------------------*/

void sensors_update(uint16_t now) {
    (void)now;
    snapshot_result = eResultOk;
}
/*----------------------------------------------------------------------------*/

EResult_t sensors_search(SSensorRom_t *roms, uint8_t capacity, uint8_t *count) {
    uint8_t index;
    uint8_t found = 0U;

    if (roms == NULL || capacity == 0U || count == NULL) {
        return eResultInvalidArgument;
    }
    if (snapshot_result != eResultOk) {
        return snapshot_result;
    }
    for (index = 0U; index < SENSOR_CAPACITY; index++) {
        found += (stub_presence & (1U << index)) != 0U;
    }
    if (found > capacity) {
        return eResultOutOfRange;
    }
    found = 0U;
    for (index = 0U; index < SENSOR_CAPACITY; index++) {
        if ((stub_presence & 1U << index) != 0U) {
            roms[found++] = stub_roms[index];
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
    if (snapshot_result != eResultOk) {
        return snapshot_result;
    }
    for (index = 0U; index < SENSOR_CAPACITY; index++) {
        if (
            (stub_presence & 1U << index) != 0U &&
            memcmp(rom.bytes, stub_roms[index].bytes, SENSOR_ROM_SIZE) == 0
        ) {
            *temperature = stub_temperatures[index];
            ret_val = eResultOk;
            break;
        }
    }
    return ret_val;
}
/******************************************************************************/
