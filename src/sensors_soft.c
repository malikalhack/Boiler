/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    sensors_soft.c
 * @version 0.1.12
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */
/******************************** Included files ******************************/
#include "boiler_config.h"

#if BOILER_USE_SOFT_1WIRE == 1
#include <stdbool.h>
#include <string.h>

#include "onewire.h"
#include "crc8.h"
#include "sensors.h"
/********************************* Definitions ********************************/

/**
 * @def ROM_SEARCH
 * @brief Discover complete device addresses through search triplets.
 */
#define ROM_SEARCH       (0xF0U)

/**
 * @def ROM_MATCH
 * @brief Select a single device by its complete ROM.
 */
#define ROM_MATCH        (0x55U)

/**
 * @def ROM_SKIP
 * @brief Select all connected devices for a broadcast conversion.
 */
#define ROM_SKIP         (0xCCU)

/**
 * @def READ_POWER
 * @brief Request the selected device's external-power status.
 */
#define READ_POWER       (0xB4U)

/**
 * @def CONVERT_TEMP
 * @brief Start temperature conversion on the selected devices.
 */
#define CONVERT_TEMP     (0x44U)

/**
 * @def READ_SCRATCHPAD
 * @brief Read the selected device's nine-byte scratchpad.
 */
#define READ_SCRATCHPAD  (0xBEU)

/**
 * @def WRITE_SCRATCHPAD
 * @brief Write alarm thresholds and resolution to volatile sensor RAM.
 */
#define WRITE_SCRATCHPAD (0x4EU)

/**
 * @def SCRATCHPAD_SIZE
 * @brief Number of scratchpad bytes, including the Dallas CRC.
 */
#define SCRATCHPAD_SIZE  (9U)

/**
 * @def RETRY_TICKS
 * @brief Pause between acquisitions at the 1 ms system tick rate.
 */
#define RETRY_TICKS      (100U)

/**
 * @def BYTE_BITS
 * @brief Number of bits in a wire byte.
 */
#define BYTE_BITS        (8U)

/**
 * @def ROM_BITS
 * @brief Number of search triplets needed for a complete ROM.
 */
#define ROM_BITS         (SENSOR_ROM_SIZE * BYTE_BITS)

/**
 * @def TEMP_MSB_INDEX
 * @brief Scratchpad offset of the temperature's most significant byte.
 */
#define TEMP_MSB_INDEX   (1U)

/**
 * @def ALARM_HIGH_INDEX
 * @brief Scratchpad offset of the high alarm threshold.
 */
#define ALARM_HIGH_INDEX (2U)

/**
 * @def ALARM_LOW_INDEX
 * @brief Scratchpad offset of the low alarm threshold.
 */
#define ALARM_LOW_INDEX  (3U)

/**
 * @def CONFIG_INDEX
 * @brief Scratchpad offset of the sensor configuration byte.
 */
#define CONFIG_INDEX    (4U)

/**
 * @def CONFIG_BYTES
 * @brief Number of bytes accepted by Write Scratchpad.
 */
#define CONFIG_BYTES    (3U)

/**
 * @def CONFIG_RESERVED_MASK
 * @brief Mask of the configuration bits fixed by the DS18B20 protocol.
 */
#define CONFIG_RESERVED_MASK  (0x9FU)

/**
 * @def CONFIG_RESERVED_VALUE
 * @brief Required value of the configuration's fixed bits.
 */
#define CONFIG_RESERVED_VALUE (0x1FU)

/**
 * @def TEMP_SIGN_MASK
 * @brief Sign bit of a 16-bit temperature wire value.
 */
#define TEMP_SIGN_MASK   (0x8000U)

/**
 * @def TEMP_MODULUS
 * @brief Modulus used to decode a signed 16-bit wire value.
 */
#define TEMP_MODULUS     (65536L)

/**
 * @def MIN_TEMPERATURE
 * @brief DS18B20 lower measurement limit in raw temperature units.
 */
#define MIN_TEMPERATURE  (-55 * SENSOR_TEMP_SCALE)

/**
 * @def MAX_TEMPERATURE
 * @brief DS18B20 upper measurement limit in raw temperature units.
 */
#define MAX_TEMPERATURE  (125 * SENSOR_TEMP_SCALE)

/**
 * @enum EPhase
 * @brief One bounded step of discovery, setup, conversion or readback.
 */
typedef enum EPhase {
    ePhaseScanReset,      /**< Reset before ROM discovery. */
    ePhaseScanCommand,    /**< Send Search ROM. */
    ePhaseScanBit,        /**< Resolve one search triplet. */
    ePhaseSelectReset,    /**< Reset before selecting a device. */
    ePhaseSelectCommand,  /**< Send Match ROM. */
    ePhaseSelectRom,      /**< Send one address byte. */
    ePhaseFunction,       /**< Send the selected device operation. */
    ePhasePower,          /**< Read external-power status. */
    ePhaseConfigure,      /**< Write one volatile setup byte. */
    ePhaseConvertReset,   /**< Reset before broadcast conversion. */
    ePhaseConvertSkip,    /**< Send Skip ROM. */
    ePhaseConvertCommand, /**< Send Convert T. */
    ePhaseConvertWait,    /**< Await conversion without blocking. */
    ePhaseConvertPoll,    /**< Consume the asynchronous completion read. */
    ePhaseScratchpad,     /**< Read one scratchpad byte. */
    ePhasePause           /**< Wait before starting another acquisition. */
} EPhase_t;

/**
 * @enum EOperation
 * @brief Selected device operation within a complete acquisition.
 */
typedef enum EOperation {
    eOperationPower,     /**< Verify external power. */
    eOperationSetup,     /**< Inspect or verify volatile configuration. */
    eOperationConfigure, /**< Preserve alarms and select 12-bit resolution. */
    eOperationRead       /**< Read a converted temperature. */
} EOperation_t;

/**
 * @struct SSnapshot
 * @brief Coherent discovered addresses and converted temperature readings.
 */
typedef struct SSnapshot {
    SSensorRom_t roms[SENSOR_CAPACITY]; /**< Addresses in search order. */
    int16_t temperatures[SENSOR_CAPACITY]; /**< Readings in raw units. */
    uint8_t count; /**< Number of entries in the snapshot. */
} SSnapshot_t;

/****************************** Module variables ******************************/

static const uint8_t operation_commands[] = {
    READ_POWER, READ_SCRATCHPAD, WRITE_SCRATCHPAD, READ_SCRATCHPAD
};
static SSnapshot_t published;
static SSnapshot_t working;
static EResult_t snapshot_result = eResultNotReady;
static EPhase_t phase;
static SSensorRom_t search_rom;
static uint8_t last_discrepancy;
static uint8_t next_discrepancy;
static uint8_t search_bit;
static uint8_t device;
static uint8_t byte_index;
static uint8_t scratchpad[SCRATCHPAD_SIZE];
static EOperation_t operation;
static bool configuration_written;
static uint8_t alarm_high;
static uint8_t alarm_low;
static uint16_t phase_tick;
static uint16_t conversion_poll_tick;

/****************************** Private functions *****************************/
/**
 * @brief Publish a complete snapshot or invalidate it on acquisition failure.
 * @param[in] result Completed acquisition result.
 * @param[in] now Tick at completion, used to schedule the next acquisition.
 */
static void publish(EResult_t result, uint16_t now) {
    if (result == eResultOk) {
        published = working;
    }
    snapshot_result = result;
    phase_tick = now;
    phase = ePhasePause;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Keep a pending phase or publish an explicit transfer failure.
 * @param[in] result Current bit, byte or triplet result.
 * @param[in] now Tick used when publishing a failure.
 * @returns True only when the transfer completed successfully.
 */
static bool transfer_complete(EResult_t result, uint16_t now) {
    if (result != eResultOk && result != eResultBusy) {
        publish(result, now);
    }
    return result == eResultOk;
}
/*----------------------------------------------------------------------------*/

/** @brief Reset discovery and per-device setup state for an acquisition. */
static void start_scan(void) {
    working.count = 0U;
    search_rom = (SSensorRom_t){{0U}};
    last_discrepancy = 0U;
    operation = eOperationPower;
    configuration_written = false;
    phase = ePhaseScanReset;
}
/*----------------------------------------------------------------------------*/

/** @brief Select the next DS18B20 while retaining unknown ROMs for diagnostics. */
static void select_device(void) {
    while (
        device < working.count &&
        working.roms[device].bytes[0] != SENSOR_FAMILY_DS18B20
    ) {
        device++;
    }
    phase = ePhaseSelectReset;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Start setup for discovered DS18B20 devices or publish an empty bus.
 * @param[in] now Tick at completion of discovery.
 */
static void finish_scan(uint16_t now) {
    if (working.count == 0U) {
        publish(eResultOk, now);
    }
    else {
        device = 0U;
        select_device();
        if (device == working.count) {
            publish(eResultOk, now);
        }
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Advance to the next device or finish the current acquisition stage.
 * @param[in] now Tick used if a converted snapshot is ready to publish.
 */
static void advance_device(uint16_t now) {
    bool converted = operation == eOperationRead;

    device++;
    operation = converted ? eOperationRead : eOperationPower;
    configuration_written = false;
    select_device();
    if (device == working.count) {
        if (converted) {
            publish(eResultOk, now);
        }
        else {
            phase = ePhaseConvertReset;
        }
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Resolve one ROM-search branch and store completed addresses.
 * @param[in] now Tick used when discovery completes or fails.
 */
static void search_triplet(uint16_t now) {
    uint8_t bits;
    bool direction;
    uint8_t index = (search_bit - 1U) / BYTE_BITS;
    uint8_t mask = 1U << ((search_bit - 1U) % BYTE_BITS);
    bool preferred = search_bit < last_discrepancy ?
        (search_rom.bytes[index] & mask) != 0U : search_bit == last_discrepancy;

    if (!transfer_complete(onewire_triplet(preferred, &bits), now)) {
        return;
    }
    direction = (bits & ONEWIRE_SEARCH_DIRECTION) != 0U;
    if ((bits & (ONEWIRE_SEARCH_ID | ONEWIRE_SEARCH_COMPLEMENT)) == 0U &&
        !direction) {
        next_discrepancy = search_bit;
    }
    if (direction) {
        search_rom.bytes[index] |= mask;
    }
    else {
        search_rom.bytes[index] &= (uint8_t)~mask;
    }
    if (search_bit++ == ROM_BITS) {
        if (crc8(search_rom.bytes, SENSOR_ROM_SIZE) != 0U ||
            search_rom.bytes[0] == 0U) {
            publish(eResultCrc, now);
            return;
        }
        working.roms[working.count++] = search_rom;
        last_discrepancy = next_discrepancy;
        if (last_discrepancy == 0U) {
            finish_scan(now);
        }
        else if (working.count == SENSOR_CAPACITY) {
            publish(eResultOutOfRange, now);
        }
        else {
            phase = ePhaseScanReset;
        }
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Validate the current scratchpad's CRC and fixed configuration bits.
 * @retval eResultOk Scratchpad is valid for setup or temperature decoding.
 * @retval eResultCrc Scratchpad CRC does not match.
 * @retval eResultBus Configuration bits are invalid.
 */
static EResult_t validate_scratchpad(void) {
    EResult_t ret_val = eResultOk;

    if (crc8(scratchpad, SCRATCHPAD_SIZE) != 0U) {
        ret_val = eResultCrc;
    }
    else if ((scratchpad[CONFIG_INDEX] & CONFIG_RESERVED_MASK) !=
             CONFIG_RESERVED_VALUE) {
        ret_val = eResultBus;
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Preserve alarm thresholds and configure verified 12-bit sensor RAM.
 * @param[in] now Tick used if setup fails.
 * @note Readback failure is reported rather than repeating writes forever.
 */
static void prepare_sensor(uint16_t now) {
    EResult_t result = validate_scratchpad();

    if (result != eResultOk) {
        publish(result, now);
        return;
    }
    if (
        configuration_written &&
        (scratchpad[CONFIG_INDEX] != SENSOR_CONFIG_12BIT ||
         scratchpad[ALARM_HIGH_INDEX] != alarm_high ||
         scratchpad[ALARM_LOW_INDEX] != alarm_low)
    ) {
        publish(eResultVerifyFailed, now);
        return;
    }
    if (scratchpad[CONFIG_INDEX] != SENSOR_CONFIG_12BIT) {
        alarm_high = scratchpad[ALARM_HIGH_INDEX];
        alarm_low = scratchpad[ALARM_LOW_INDEX];
        operation = eOperationConfigure;
        select_device();
    }
    else {
        advance_device(now);
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Decode the converted, verified 12-bit scratchpad temperature.
 * @param[out] temperature Destination in signed 1/16 C units.
 * @returns CRC, bus, setup-readback or successful decoding result.
 */
static EResult_t decode_temperature(int16_t *temperature) {
    uint16_t raw;
    EResult_t ret_val = validate_scratchpad();

    if (ret_val != eResultOk) {
        return ret_val;
    }
    if (scratchpad[CONFIG_INDEX] != SENSOR_CONFIG_12BIT) {
        return eResultVerifyFailed;
    }
    raw = (uint16_t)scratchpad[0] |
          (uint16_t)scratchpad[TEMP_MSB_INDEX] << BYTE_BITS;
    /* Convert the signed wire value without an out-of-range unsigned cast. */
    *temperature = (raw & TEMP_SIGN_MASK) != 0U ?
        (int16_t)((int32_t)raw - TEMP_MODULUS) : (int16_t)raw;
    if (*temperature < MIN_TEMPERATURE || *temperature > MAX_TEMPERATURE) {
        ret_val = eResultBus;
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Validate a complete setup or temperature scratchpad and advance.
 * @param[in] now Tick used when publishing an acquisition result.
 */
static void finish_scratchpad(uint16_t now) {
    EResult_t result;

    if (operation == eOperationSetup) {
        prepare_sensor(now);
    }
    else {
        result = decode_temperature(&working.temperatures[device]);
        if (result != eResultOk) {
            publish(result, now);
        }
        else {
            advance_device(now);
        }
    }
}
/*----------------------------------------------------------------------------*/

/********************** Application Programming Interface *********************/

void sensors_init(void) {
    onewire_init();
    sensors_refresh();
}
/*----------------------------------------------------------------------------*/

void sensors_refresh(void) {
    snapshot_result = eResultBusy;
    start_scan();
}
/*----------------------------------------------------------------------------*/

void sensors_update(uint16_t now) {
    EResult_t result;
    bool bit;

    switch (phase) {
        case ePhaseScanReset:
            result = onewire_reset();
            if (result == eResultBusy) {
                break;
            }
            if (result == eResultNotFound && working.count == 0U) {
                publish(eResultOk, now);
            }
            else if (result != eResultOk) {
                publish(eResultBus, now);
            }
            else {
                phase = ePhaseScanCommand;
            }
            break;
        case ePhaseScanCommand:
            if (transfer_complete(onewire_write_byte(ROM_SEARCH), now)) {
                search_bit = 1U;
                next_discrepancy = 0U;
                phase = ePhaseScanBit;
            }
            break;
        case ePhaseScanBit:
            search_triplet(now);
            break;
        case ePhaseSelectReset:
            result = onewire_reset();
            if (result == eResultBusy) {
                break;
            }
            if (result != eResultOk) {
                publish(eResultBus, now);
            }
            else {
                phase = ePhaseSelectCommand;
            }
            break;
        case ePhaseSelectCommand:
            if (transfer_complete(onewire_write_byte(ROM_MATCH), now)) {
                byte_index = 0U;
                phase = ePhaseSelectRom;
            }
            break;
        case ePhaseSelectRom:
            if (transfer_complete(onewire_write_byte(
                    working.roms[device].bytes[byte_index]), now)) {
                if (++byte_index == SENSOR_ROM_SIZE) {
                    phase = ePhaseFunction;
                }
            }
            break;
        case ePhaseFunction:
            if (transfer_complete(onewire_write_byte(
                    operation_commands[operation]), now)) {
                byte_index = 0U;
                phase = operation == eOperationPower ? ePhasePower :
                    operation == eOperationConfigure ? ePhaseConfigure :
                    ePhaseScratchpad;
            }
            break;
        case ePhasePower:
            if (transfer_complete(onewire_read_bit(&bit), now)) {
                if (!bit) {
                    publish(eResultPower, now);
                }
                else {
                    operation = eOperationSetup;
                    select_device();
                }
            }
            break;
        case ePhaseConfigure:
            if (transfer_complete(onewire_write_byte(
                    byte_index == 0U ? alarm_high :
                    byte_index == 1U ? alarm_low : SENSOR_CONFIG_12BIT), now)) {
                if (++byte_index == CONFIG_BYTES) {
                    configuration_written = true;
                    operation = eOperationSetup;
                    select_device();
                }
            }
            break;
        case ePhaseConvertReset:
            result = onewire_reset();
            if (result == eResultBusy) {
                break;
            }
            if (result != eResultOk) {
                publish(eResultBus, now);
            }
            else {
                phase = ePhaseConvertSkip;
            }
            break;
        case ePhaseConvertSkip:
            if (transfer_complete(onewire_write_byte(ROM_SKIP), now)) {
                phase = ePhaseConvertCommand;
            }
            break;
        case ePhaseConvertCommand:
            if (transfer_complete(onewire_write_byte(CONVERT_TEMP), now)) {
                phase_tick = now;
                phase = ePhaseConvertWait;
            }
            break;
        case ePhaseConvertWait:
            if ((uint16_t)(now - phase_tick) < SENSOR_CONVERSION_TICKS) {
                break;
            }
            conversion_poll_tick = now;
            phase = ePhaseConvertPoll;
            /* fall through */
        case ePhaseConvertPoll:
            if (transfer_complete(onewire_read_bit(&bit), now)) {
                if (bit) {
                    operation = eOperationRead;
                    device = 0U;
                    select_device();
                }
                else if ((uint16_t)(conversion_poll_tick - phase_tick) >=
                         SENSOR_TIMEOUT_TICKS) {
                    publish(eResultTimeout, now);
                }
                else {
                    phase = ePhaseConvertWait;
                }
            }
            break;
        case ePhaseScratchpad:
            if (!transfer_complete(onewire_read_byte(
                    &scratchpad[byte_index]), now)) {
                break;
            }
            if (++byte_index == SCRATCHPAD_SIZE) {
                finish_scratchpad(now);
            }
            break;
        case ePhasePause:
            if ((uint16_t)(now - phase_tick) >= RETRY_TICKS) {
                start_scan();
            }
            break;
    }
}
/*----------------------------------------------------------------------------*/

EResult_t sensors_search(SSensorRom_t *roms, uint8_t capacity, uint8_t *count) {
    if (roms == NULL || capacity == 0U || count == NULL) {
        return eResultInvalidArgument;
    }
    if (snapshot_result != eResultOk) {
        return snapshot_result;
    }
    if (published.count > capacity) {
        return eResultOutOfRange;
    }
    memcpy(roms, published.roms, published.count * sizeof(*roms));
    *count = published.count;
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
    for (index = 0U; index < published.count; index++) {
        if (
            rom.bytes[0] == SENSOR_FAMILY_DS18B20 &&
            memcmp(rom.bytes, published.roms[index].bytes, SENSOR_ROM_SIZE) == 0
        ) {
            *temperature = published.temperatures[index];
            ret_val = eResultOk;
            break;
        }
    }
    return ret_val;
}
/******************************************************************************/
#endif /* BOILER_USE_SOFT_1WIRE */
