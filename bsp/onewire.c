/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    onewire.c
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "boiler_config.h"

#if BOILER_USE_SOFT_1WIRE == 1
#include <avr/io.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "onewire.h"
/********************************* Definitions ********************************/

/**
 * @def ONEWIRE_MASK
 * @brief Port D bit used exclusively for the software open-drain bus.
 */
#define ONEWIRE_MASK (_BV(PD2))

/**
 * @def RESET_LOW_US
 * @brief Minimum standard-speed reset pulse duration.
 */
#define RESET_LOW_US          (480U)

/**
 * @def PRESENCE_SAMPLE_US
 * @brief Delay from reset release to the presence sample.
 */
#define PRESENCE_SAMPLE_US    (70U)

/**
 * @def RESET_RECOVERY_US
 * @brief Recovery after sampling the presence pulse.
 */
#define RESET_RECOVERY_US     (410U)

/**
 * @def WRITE_ONE_LOW_US
 * @brief Low pulse for a Write 1 slot.
 */
#define WRITE_ONE_LOW_US      (6U)

/**
 * @def WRITE_ONE_RECOVERY_US
 * @brief Released interval completing a Write 1 slot.
 */
#define WRITE_ONE_RECOVERY_US (64U)

/**
 * @def WRITE_ZERO_LOW_US
 * @brief Low pulse for a Write 0 slot.
 */
#define WRITE_ZERO_LOW_US      (60U)

/**
 * @def WRITE_ZERO_RECOVERY_US
 * @brief Released interval completing a Write 0 slot.
 */
#define WRITE_ZERO_RECOVERY_US (10U)

/**
 * @def READ_LOW_US
 * @brief Low pulse initiating a read slot.
 */
#define READ_LOW_US           (3U)

/**
 * @def READ_SAMPLE_US
 * @brief Delay from read-slot release to sampling the bus.
 */
#define READ_SAMPLE_US        (8U)

/**
 * @def READ_RECOVERY_US
 * @brief Recovery after sampling a read slot.
 */
#define READ_RECOVERY_US      (59U)

/********************** Application Programming Interface *********************/

void onewire_init(void) {
    DDRD &= (uint8_t)~ONEWIRE_MASK;
    PORTD &= (uint8_t)~ONEWIRE_MASK;
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_reset(void) {
    bool present;

    if ((PIND & ONEWIRE_MASK) == 0U) {
        return eResultBus;
    }
    DDRD |= ONEWIRE_MASK;
    _delay_us(RESET_LOW_US);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        DDRD &= (uint8_t)~ONEWIRE_MASK;
        _delay_us(PRESENCE_SAMPLE_US);
        present = (PIND & ONEWIRE_MASK) == 0U;
    }
    _delay_us(RESET_RECOVERY_US);
    if ((PIND & ONEWIRE_MASK) == 0U) {
        return eResultBus;
    }
    return present ? eResultOk : eResultNotFound;
}
/*----------------------------------------------------------------------------*/

void onewire_write_bit(bool value) {
    if (value) {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            DDRD |= ONEWIRE_MASK;
            _delay_us(WRITE_ONE_LOW_US);
            DDRD &= (uint8_t)~ONEWIRE_MASK;
        }
        _delay_us(WRITE_ONE_RECOVERY_US);
    }
    else {
        ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
            DDRD |= ONEWIRE_MASK;
            _delay_us(WRITE_ZERO_LOW_US);
            DDRD &= (uint8_t)~ONEWIRE_MASK;
        }
        _delay_us(WRITE_ZERO_RECOVERY_US);
    }
}
/*----------------------------------------------------------------------------*/

bool onewire_read_bit(void) {
    bool value;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        DDRD |= ONEWIRE_MASK;
        _delay_us(READ_LOW_US);
        DDRD &= (uint8_t)~ONEWIRE_MASK;
        _delay_us(READ_SAMPLE_US);
        value = (PIND & ONEWIRE_MASK) != 0U;
    }
    _delay_us(READ_RECOVERY_US);
    return value;
}
/******************************************************************************/
#endif /* BOILER_USE_SOFT_1WIRE */
