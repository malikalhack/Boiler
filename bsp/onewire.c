/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    onewire.c
 * @version 0.1.9
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "boiler_config.h"

#if BOILER_USE_SOFT_1WIRE == 1
#include <avr/interrupt.h>
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
 * @def RESET_TIMER_PRESCALER
 * @brief Timer0 clock divider; the shared Timer1 prescaler is not reset.
 */
#define RESET_TIMER_PRESCALER (64UL)

/**
 * @def RESET_TIMER_COMPARE
 * @brief Round up a wait and allow for an uncertain first prescaler tick.
 * @param[in] us Minimum wait in microseconds.
 */
#define RESET_TIMER_COMPARE(us) \
    ((F_CPU / RESET_TIMER_PRESCALER * (us) + 999999UL) / 1000000UL + 1UL)

/**
 * @def RESET_LOW_COMPARE
 * @brief Timer0 compare value guaranteeing at least 480 us of Reset low.
 */
#define RESET_LOW_COMPARE RESET_TIMER_COMPARE(RESET_LOW_US)

/**
 * @def RESET_RECOVERY_COMPARE
 * @brief Timer0 compare value guaranteeing at least 410 us of recovery.
 */
#define RESET_RECOVERY_COMPARE RESET_TIMER_COMPARE(RESET_RECOVERY_US)

#if RESET_LOW_COMPARE > 255UL || RESET_RECOVERY_COMPARE > 255UL
#error "Reset waits do not fit Timer0"
#endif

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

/**
 * @enum EResetPhase
 * @brief Timer-driven Reset phases and a result awaiting foreground polling.
 */
typedef enum EResetPhase {
    eResetIdle,     /**< No Reset is in progress. */
    eResetLow,      /**< Timer0 is holding the Reset pulse low. */
    eResetRecovery, /**< Presence was sampled; the bus is recovering. */
    eResetDone      /**< Reset result is ready for its caller. */
} EResetPhase_t;

/****************************** Module variables ******************************/

static volatile EResetPhase_t reset_phase;
static volatile EResult_t reset_result;
static bool reset_present;

/****************************** Private functions *****************************/

/**
 * @brief Arm a Timer0 CTC wait without changing Timer1 or its prescaler.
 * @param[in] compare Compare threshold for the next Reset phase.
 * @note Call with interrupts masked; OC0 stays disconnected from GPIO.
 */
static void arm_reset_timer(uint8_t compare) {
    TCNT0 = 0U;
    OCR0 = compare;
    TIFR = _BV(OCF0);
    TIMSK |= _BV(OCIE0);
    TCCR0 = _BV(WGM01) | _BV(CS01) | _BV(CS00);
}
/*----------------------------------------------------------------------------*/

/********************** Application Programming Interface *********************/

void onewire_init(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TCCR0 = 0U;
        TIMSK &= (uint8_t)~(_BV(OCIE0) | _BV(TOIE0));
        TIFR = _BV(OCF0) | _BV(TOV0);
        reset_phase = eResetIdle;
        DDRD &= (uint8_t)~ONEWIRE_MASK;
        PORTD &= (uint8_t)~ONEWIRE_MASK;
    }
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_reset(void) {
    EResult_t result = eResultBusy;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (reset_phase == eResetDone) {
            result = reset_result;
            reset_phase = eResetIdle;
        }
        else if (reset_phase == eResetIdle) {
            if ((PIND & ONEWIRE_MASK) == 0U) {
                result = eResultBus;
            }
            else {
                reset_phase = eResetLow;
                DDRD |= ONEWIRE_MASK;
                arm_reset_timer(RESET_LOW_COMPARE);
            }
        }
    }
    return result;
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
/*----------------------------------------------------------------------------*/

/**
 * @brief Release Reset and sample presence, then complete timed recovery.
 * @note Only the verified 70 us presence window still uses a software wait.
 * No display or scheduler work is performed in this interrupt handler.
 */
ISR(TIMER0_COMP_vect) {
    TCCR0 = 0U;
    if (reset_phase == eResetLow) {
        DDRD &= (uint8_t)~ONEWIRE_MASK;
        _delay_us(PRESENCE_SAMPLE_US);
        reset_present = (PIND & ONEWIRE_MASK) == 0U;
        reset_phase = eResetRecovery;
        arm_reset_timer(RESET_RECOVERY_COMPARE);
    }
    else {
        TIMSK &= (uint8_t)~_BV(OCIE0);
        reset_result = (PIND & ONEWIRE_MASK) == 0U ? eResultBus :
            reset_present ? eResultOk : eResultNotFound;
        reset_phase = eResetDone;
    }
}
/******************************************************************************/
#endif /* BOILER_USE_SOFT_1WIRE */
