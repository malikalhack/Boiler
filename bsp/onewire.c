/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    onewire.c
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include "boiler_config.h"

#if BOILER_USE_SOFT_1WIRE == 1
#include <stddef.h>

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
 * @brief Timer wait before presence sampling, leaving room for ISR overhead.
 * @note The physical sample must stay within the 60..75 us presence overlap.
 */
#define PRESENCE_SAMPLE_US    62U

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
 * @def SLOT_TIMER_PRESCALER
 * @brief Finer Timer0 clock for presence and standard-speed bit slots.
 */
#define SLOT_TIMER_PRESCALER 8UL

/**
 * @def TIMER_COMPARE
 * @brief Round up a wait and allow for an uncertain first prescaler tick.
 * @param[in] us Minimum wait in microseconds.
 * @param[in] divider Timer0 prescaler divisor.
 */
#define TIMER_COMPARE(us, divider) \
    ((F_CPU / (divider) * (us) + 999999UL) / 1000000UL + 1UL)

/**
 * @def RESET_LOW_COMPARE
 * @brief Timer0 compare value guaranteeing at least 480 us of Reset low.
 */
#define RESET_LOW_COMPARE TIMER_COMPARE(RESET_LOW_US, RESET_TIMER_PRESCALER)

/**
 * @def RESET_RECOVERY_COMPARE
 * @brief Timer0 compare value guaranteeing at least 410 us of recovery.
 */
#define RESET_RECOVERY_COMPARE \
    TIMER_COMPARE(RESET_RECOVERY_US, RESET_TIMER_PRESCALER)

/**
 * @def PRESENCE_COMPARE
 * @brief Fine-clock compare for the timer-driven presence sample.
 */
#define PRESENCE_COMPARE TIMER_COMPARE(PRESENCE_SAMPLE_US, SLOT_TIMER_PRESCALER)

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
 * @def WRITE_ZERO_COMPARE
 * @brief Fine-clock compare for a Write 0 low interval.
 */
#define WRITE_ZERO_COMPARE TIMER_COMPARE(WRITE_ZERO_LOW_US, SLOT_TIMER_PRESCALER)

/**
 * @def WRITE_ONE_RECOVERY_COMPARE
 * @brief Fine-clock compare for Write 1 recovery.
 */
#define WRITE_ONE_RECOVERY_COMPARE \
    TIMER_COMPARE(WRITE_ONE_RECOVERY_US, SLOT_TIMER_PRESCALER)

/**
 * @def WRITE_ZERO_RECOVERY_COMPARE
 * @brief Fine-clock compare for Write 0 recovery.
 */
#define WRITE_ZERO_RECOVERY_COMPARE \
    TIMER_COMPARE(WRITE_ZERO_RECOVERY_US, SLOT_TIMER_PRESCALER)

/**
 * @def READ_RECOVERY_COMPARE
 * @brief Fine-clock compare for read-slot recovery.
 */
#define READ_RECOVERY_COMPARE \
    TIMER_COMPARE(READ_RECOVERY_US, SLOT_TIMER_PRESCALER)

/**
 * @def BYTE_BITS
 * @brief Number of least-significant-bit-first slots in a byte.
 */
#define BYTE_BITS 8U

/**
 * @def SEARCH_SLOTS
 * @brief Two read slots and one branch-selection slot in a search triplet.
 */
#define SEARCH_SLOTS 3U

#if RESET_LOW_COMPARE > UINT8_MAX || RESET_RECOVERY_COMPARE > UINT8_MAX
#error "Reset waits do not fit Timer0"
#endif /* RESET_LOW_COMPARE, RESET_RECOVERY_COMPARE */

#if PRESENCE_COMPARE > UINT8_MAX || WRITE_ZERO_COMPARE > UINT8_MAX || \
    WRITE_ONE_RECOVERY_COMPARE > UINT8_MAX || \
    WRITE_ZERO_RECOVERY_COMPARE > UINT8_MAX || READ_RECOVERY_COMPARE > UINT8_MAX
#error "Slot waits do not fit Timer0"
#endif /* PRESENCE_COMPARE, slot compares */

/**
 * @enum ETransfer
 * @brief Operation owning Timer0 until its result is consumed or reset.
 */
typedef enum ETransfer {
    eTransferReset,     /**< Reset and presence detection. */
    eTransferWriteBit,  /**< One write slot. */
    eTransferReadBit,   /**< One read slot. */
    eTransferWriteByte, /**< Eight autonomous write slots. */
    eTransferReadByte,  /**< Eight autonomous read slots. */
    eTransferTriplet    /**< Two reads and automatic search-branch selection. */
} ETransfer_t;

/**
 * @enum EWirePhase
 * @brief Timer-driven bus phase or a result awaiting foreground consumption.
 */
typedef enum EWirePhase {
    eWireIdle,          /**< No operation owns the bus. */
    eWireResetLow,      /**< Hold the Reset pulse low. */
    eWirePresence,      /**< Await the presence sample after Reset release. */
    eWireResetRecovery, /**< Await final Reset recovery. */
    eWireWriteLow,      /**< Hold a Write 0 low interval. */
    eWireSlotRecovery,  /**< Recover before the next slot or completion. */
    eWireDone           /**< Operation result is ready for its caller. */
} EWirePhase_t;

/****************************** Module variables ******************************/

static volatile EWirePhase_t wire_phase;
static volatile ETransfer_t transfer;
static volatile EResult_t transfer_result;
static volatile uint8_t transfer_data;
static uint8_t transfer_mask;
static uint8_t slots_remaining;
static bool preferred_direction;
static bool reset_present;

/****************************** Private functions *****************************/

/**
 * @brief Arm a Timer0 CTC wait without resetting the shared prescaler.
 * @param[in] compare Compare threshold for the next bus phase.
 * @param[in] clock Clock-selection bits for /8 or /64.
 * @note Call with interrupts masked; OC0 stays disconnected from GPIO.
 */
static void arm_timer(uint8_t compare, uint8_t clock) {
    TCNT0 = 0U;
    OCR0 = compare;
    TIFR = _BV(OCF0);
    TIMSK |= _BV(OCIE0);
    TCCR0 = _BV(WGM01) | clock;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Stop bus interrupts and latch one completed operation result.
 * @param[in] result Explicit bus result to consume in the foreground.
 * @note Call with interrupts masked and PD2 released.
 */
static void finish_transfer(EResult_t result) {
    TIMSK &= (uint8_t)~_BV(OCIE0);
    transfer_result = result;
    wire_phase = eWireDone;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Start one slot, retaining only its short critical software interval.
 * @note Call with interrupts masked and Timer0 stopped.
 */
static void start_slot(void) {
    bool reading = transfer == eTransferReadBit || transfer == eTransferReadByte;

    if (transfer == eTransferTriplet) {
        reading = transfer_mask != ONEWIRE_SEARCH_DIRECTION;
    }
    if (reading) {
        DDRD |= ONEWIRE_MASK;
        _delay_us(READ_LOW_US);
        DDRD &= (uint8_t)~ONEWIRE_MASK;
        _delay_us(READ_SAMPLE_US);
        if ((PIND & ONEWIRE_MASK) != 0U) {
            transfer_data |= transfer_mask;
        }
        wire_phase = eWireSlotRecovery;
        arm_timer(READ_RECOVERY_COMPARE, _BV(CS01));
    }
    else if ((transfer_data & transfer_mask) != 0U) {
        DDRD |= ONEWIRE_MASK;
        _delay_us(WRITE_ONE_LOW_US);
        DDRD &= (uint8_t)~ONEWIRE_MASK;
        wire_phase = eWireSlotRecovery;
        arm_timer(WRITE_ONE_RECOVERY_COMPARE, _BV(CS01));
    }
    else {
        wire_phase = eWireWriteLow;
        DDRD |= ONEWIRE_MASK;
        arm_timer(WRITE_ZERO_COMPARE, _BV(CS01));
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Continue autonomous slots or finish a byte, bit or search triplet.
 * @note Call after recovery with interrupts masked and Timer0 stopped.
 */
static void next_slot(void) {
    bool direction;

    slots_remaining--;
    if (slots_remaining == 0U) {
        finish_transfer(eResultOk);
    }
    else {
        if (transfer == eTransferTriplet && slots_remaining == 1U) {
            if (transfer_data ==
                (ONEWIRE_SEARCH_ID | ONEWIRE_SEARCH_COMPLEMENT)) {
                finish_transfer(eResultBus);
                return;
            }
            direction = transfer_data == ONEWIRE_SEARCH_ID;
            if (transfer_data == 0U) {
                direction = preferred_direction;
            }
            if (direction) {
                transfer_data |= ONEWIRE_SEARCH_DIRECTION;
            }
        }
        transfer_mask <<= 1U;
        start_slot();
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Initialize one Reset, bit, byte or triplet operation.
 * @param[in] requested Operation that now owns Timer0.
 * @param[in] value Write data or preferred search direction.
 * @note Call with interrupts masked and Timer0 stopped.
 */
static void start_transfer(ETransfer_t requested, uint8_t value) {
    transfer = requested;
    if (requested == eTransferReset) {
        wire_phase = eWireResetLow;
        DDRD |= ONEWIRE_MASK;
        arm_timer(RESET_LOW_COMPARE, _BV(CS01) | _BV(CS00));
    }
    else {
        preferred_direction = value != 0U;
        transfer_data = requested == eTransferWriteByte ||
            requested == eTransferWriteBit ? value : 0U;
        transfer_mask = 1U;
        slots_remaining = requested == eTransferWriteByte ||
            requested == eTransferReadByte ? BYTE_BITS :
            requested == eTransferTriplet ? SEARCH_SLOTS : 1U;
        start_slot();
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Start or consume a single operation without restarting timed phases.
 * @param[in] requested Operation requested by the foreground caller.
 * @param[in] value Write data or preferred search direction.
 * @param[out] received Read data, or NULL for operations without output.
 * @returns Busy, successful completion or an explicit bus fault.
 * @note Reset may preempt another operation, including an unconsumed result.
 */
static EResult_t start_or_poll(
    ETransfer_t requested, uint8_t value, uint8_t *received
) {
    EResult_t ret_val = eResultBusy;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        if (transfer == requested && wire_phase == eWireDone) {
            ret_val = transfer_result;
            if (ret_val == eResultOk && received != NULL) {
                *received = transfer_data;
            }
            wire_phase = eWireIdle;
        }
        else if (
            requested == eTransferReset && transfer != eTransferReset &&
            wire_phase != eWireIdle
        ) {
            TCCR0 = 0U;
            start_transfer(requested, value);
        }
        else if (wire_phase == eWireIdle) {
            if ((PIND & ONEWIRE_MASK) == 0U) {
                ret_val = eResultBus;
            }
            else {
                start_transfer(requested, value);
            }
        }
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

/********************** Application Programming Interface *********************/

void onewire_init(void) {
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        TCCR0 = 0U;
        TIMSK &= (uint8_t)~(_BV(OCIE0) | _BV(TOIE0));
        TIFR = _BV(OCF0) | _BV(TOV0);
        wire_phase = eWireIdle;
        transfer = eTransferReset;
        DDRD &= (uint8_t)~ONEWIRE_MASK;
        PORTD &= (uint8_t)~ONEWIRE_MASK;
    }
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_reset(void) {
    return start_or_poll(eTransferReset, 0U, NULL);
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_write_bit(bool value) {
    return start_or_poll(eTransferWriteBit, value, NULL);
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_read_bit(bool *value) {
    uint8_t data;
    EResult_t ret_val;

    if (value == NULL) {
        return eResultInvalidArgument;
    }
    ret_val = start_or_poll(eTransferReadBit, 0U, &data);
    if (ret_val == eResultOk) {
        *value = data != 0U;
    }
    return ret_val;
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_write_byte(uint8_t value) {
    return start_or_poll(eTransferWriteByte, value, NULL);
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_read_byte(uint8_t *value) {
    if (value == NULL) {
        return eResultInvalidArgument;
    }
    return start_or_poll(eTransferReadByte, 0U, value);
}
/*----------------------------------------------------------------------------*/

EResult_t onewire_triplet(bool direction, uint8_t *bits) {
    if (bits == NULL) {
        return eResultInvalidArgument;
    }
    return start_or_poll(eTransferTriplet, direction, bits);
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Complete timed bus phases and autonomously advance transfer slots.
 * @note Presence is sampled early to limit interrupt-entry timing overhead.
 * No display or scheduler work is performed in this interrupt handler.
 */
ISR(TIMER0_COMP_vect) {
    bool high = (PIND & ONEWIRE_MASK) != 0U;

    TCCR0 = 0U;
    switch (wire_phase) {
        case eWireResetLow:
            DDRD &= (uint8_t)~ONEWIRE_MASK;
            wire_phase = eWirePresence;
            arm_timer(PRESENCE_COMPARE, _BV(CS01));
            break;
        case eWirePresence:
            reset_present = !high;
            wire_phase = eWireResetRecovery;
            arm_timer(RESET_RECOVERY_COMPARE, _BV(CS01) | _BV(CS00));
            break;
        case eWireResetRecovery:
            finish_transfer(!high ? eResultBus :
                            reset_present ? eResultOk : eResultNotFound);
            break;
        case eWireWriteLow:
            DDRD &= (uint8_t)~ONEWIRE_MASK;
            wire_phase = eWireSlotRecovery;
            arm_timer(WRITE_ZERO_RECOVERY_COMPARE, _BV(CS01));
            break;
        case eWireSlotRecovery:
            if (!high) {
                finish_transfer(eResultBus);
            }
            else {
                next_slot();
            }
            break;
        case eWireIdle:
        case eWireDone:
            TIMSK &= (uint8_t)~_BV(OCIE0);
            break;
    }
}
/******************************************************************************/
#endif /* BOILER_USE_SOFT_1WIRE */
