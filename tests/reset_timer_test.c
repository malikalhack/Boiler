/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    reset_timer_test.c
 * @version 0.1.10
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <assert.h>
#include <stdio.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "boiler_config.h"
#include "onewire.h"
/********************************* Definitions ********************************/

/**
 * @def TIMER_CLOCK_DIVIDER
 * @brief Independently modeled free-running Timer0/Timer1 prescaler.
 */
#define TIMER_CLOCK_DIVIDER 64U

/**
 * @def TIMER_MODE
 * @brief Timer0 CTC with a divided-by-64 clock and no GPIO output.
 */
#define TIMER_MODE (_BV(WGM01) | _BV(CS01) | _BV(CS00))

/**
 * @def REGISTER_ALL_BITS
 * @brief All bits set in an eight-bit hardware register.
 */
#define REGISTER_ALL_BITS 0xFFU

/**
 * @def OTHER_TIMER_INTERRUPTS
 * @brief Interrupt-enable bits not owned by the Timer0 Reset driver.
 */
#define OTHER_TIMER_INTERRUPTS ((uint8_t)~(_BV(OCIE0) | _BV(TOIE0)))

/**
 * @def GPIO_DIRECTION_SENTINEL
 * @brief Unrelated Port D directions that the driver must preserve.
 */
#define GPIO_DIRECTION_SENTINEL 0xF0U

/**
 * @def GPIO_OUTPUT_SENTINEL
 * @brief Unrelated Port D output levels that the driver must preserve.
 */
#define GPIO_OUTPUT_SENTINEL 0xB0U

/**
 * @def PRESCALER_SENTINEL
 * @brief Shared prescaler-control state that the driver must preserve.
 */
#define PRESCALER_SENTINEL 0xA0U

/**
 * @def SYSTEM_TIMER_CONTROL_A_SENTINEL
 * @brief Timer1 control register A state used to detect accidental writes.
 */
#define SYSTEM_TIMER_CONTROL_A_SENTINEL 0x12U

/**
 * @def SYSTEM_TIMER_CONTROL_B_SENTINEL
 * @brief Timer1 control register B state used to detect accidental writes.
 */
#define SYSTEM_TIMER_CONTROL_B_SENTINEL 0x34U

/**
 * @def SYSTEM_TIMER_COUNTER_SENTINEL
 * @brief Timer1 counter value used to detect accidental writes.
 */
#define SYSTEM_TIMER_COUNTER_SENTINEL 0x1234U

/**
 * @def SYSTEM_TIMER_COMPARE_SENTINEL
 * @brief Timer1 compare value used to detect accidental writes.
 */
#define SYSTEM_TIMER_COMPARE_SENTINEL 0x5678U

/**
 * @def RESET_LOW_COMPARE
 * @brief Independently expected Timer0 compare for the Reset low phase.
 */
#define RESET_LOW_COMPARE 112U

/**
 * @def RESET_RECOVERY_COMPARE
 * @brief Independently expected Timer0 compare for the recovery phase.
 */
#define RESET_RECOVERY_COMPARE 96U

/**
 * @def RESET_LOW_MIN_US
 * @brief Minimum allowed Reset low duration in microseconds.
 */
#define RESET_LOW_MIN_US 480U

/**
 * @def RESET_LOW_MAX_US
 * @brief Exclusive upper bound on the modeled Reset low duration.
 */
#define RESET_LOW_MAX_US 500U

/**
 * @def RESET_RECOVERY_MIN_US
 * @brief Minimum recovery duration after presence sampling in microseconds.
 */
#define RESET_RECOVERY_MIN_US 410U

/**
 * @def PRESENCE_SAMPLE_US
 * @brief Expected software presence-sample wait in microseconds.
 */
#define PRESENCE_SAMPLE_US 70U

/**
 * @def PRESENCE_END_US
 * @brief Modeled presence-pulse end after bus release in microseconds.
 */
#define PRESENCE_END_US 150U

/**
 * @def MICROSECONDS_PER_SECOND
 * @brief Conversion factor from microseconds to seconds.
 */
#define MICROSECONDS_PER_SECOND 1000000UL

/**
 * @def MILLISECONDS_PER_SECOND
 * @brief Conversion factor from milliseconds to seconds.
 */
#define MILLISECONDS_PER_SECOND 1000U

/**
 * @def POLL_CYCLE_LIMIT
 * @brief Maximum foreground polling cycles before a failed timer assertion.
 */
#define POLL_CYCLE_LIMIT 8000U

/**
 * @def RESET_INTERRUPT_COUNT
 * @brief Expected interrupts for the low and recovery phases of one Reset.
 */
#define RESET_INTERRUPT_COUNT 2U

/**
 * @def POST_RESET_WAIT_MS
 * @brief Delay before consuming a completed Reset result in milliseconds.
 */
#define POST_RESET_WAIT_MS 3U

/**
 * @def CANCEL_WAIT_MS
 * @brief Observation interval after Reset cancellation in milliseconds.
 */
#define CANCEL_WAIT_MS 2U

/**
 * @def UNPOLLED_WAIT_MS
 * @brief Time allowed for Reset completion without polling in milliseconds.
 */
#define UNPOLLED_WAIT_MS 4U

/**
 * @def CANCEL_CLOCK_PHASE
 * @brief Arbitrary nonzero shared-clock phase used by cancellation tests.
 */
#define CANCEL_CLOCK_PHASE 17U

/**
 * @def CANCEL_LOW_CYCLES
 * @brief CPU cycles elapsed before cancellation during the low phase.
 */
#define CANCEL_LOW_CYCLES 100U

/****************************** Module variables ******************************/

volatile uint8_t DDRD;
volatile uint8_t PORTD;
volatile uint8_t PIND;
volatile uint8_t TCCR0;
volatile uint8_t TCNT0;
volatile uint8_t OCR0;
volatile uint8_t TIMSK;
volatile uint8_t TIFR;
volatile uint8_t SFIOR;
volatile uint8_t TCCR1A;
volatile uint8_t TCCR1B;
volatile uint16_t TCNT1;
volatile uint16_t OCR1A;

static bool interrupts_enabled;
static bool device_present;
static bool recovery_fault;
static bool released;
static uint32_t cpu_cycles;
static uint32_t pulse_started;
static uint32_t pulse_cycles;
static uint32_t presence_end;
static unsigned int delay_calls;
static unsigned int timer_interrupts;

/****************************** Private functions *****************************/

/**
 * @brief Advance independent timer clocks, including external presence.
 * @param[in] count Number of CPU cycles to advance.
 */
static void advance_cycles(uint32_t count) {
    bool previous;

    while (count-- != 0U) {
        cpu_cycles++;
        if ((DDRD & _BV(PD2)) != 0U) {
            PIND &= (uint8_t)~_BV(PD2);
        }
        else if (!released) {
            PIND |= _BV(PD2);
        }
        else if (!recovery_fault && cpu_cycles >= presence_end) {
            PIND |= _BV(PD2);
        }
        if (
            TCCR0 != TIMER_MODE ||
            cpu_cycles % TIMER_CLOCK_DIVIDER != 0U
        ) {
            continue;
        }
        TCNT0++;
        if (TCNT0 != OCR0) {
            continue;
        }
        TCNT0 = 0U;
        if ((TIMSK & _BV(OCIE0)) != 0U) {
            assert(interrupts_enabled);
            previous = interrupts_enabled;
            interrupts_enabled = false;
            timer_interrupts++;
            TIMER0_COMP_vect();
            interrupts_enabled = previous;
        }
    }
}
/*----------------------------------------------------------------------------*/

/** @brief Check GPIO ownership, shared-prescaler and system-timer isolation. */
static void check_isolation(void) {
    assert((DDRD & (uint8_t)~_BV(PD2)) == GPIO_DIRECTION_SENTINEL);
    assert(PORTD == GPIO_OUTPUT_SENTINEL);
    assert((TIMSK & OTHER_TIMER_INTERRUPTS) == OTHER_TIMER_INTERRUPTS);
    assert(SFIOR == PRESCALER_SENTINEL);
    assert(TCCR1A == SYSTEM_TIMER_CONTROL_A_SENTINEL);
    assert(TCCR1B == SYSTEM_TIMER_CONTROL_B_SENTINEL);
    assert(TCNT1 == SYSTEM_TIMER_COUNTER_SENTINEL);
    assert(OCR1A == SYSTEM_TIMER_COMPARE_SENTINEL);
    assert(interrupts_enabled);
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Initialize hardware sentinels at an arbitrary shared-clock phase.
 * @param[in] clock_phase Initial phase within the free-running prescaler.
 */
static void prepare(uint8_t clock_phase) {
    DDRD = GPIO_DIRECTION_SENTINEL | _BV(PD2);
    PORTD = GPIO_OUTPUT_SENTINEL | _BV(PD2);
    PIND = REGISTER_ALL_BITS;
    TIMSK = REGISTER_ALL_BITS;
    TCCR0 = REGISTER_ALL_BITS;
    SFIOR = PRESCALER_SENTINEL;
    TCCR1A = SYSTEM_TIMER_CONTROL_A_SENTINEL;
    TCCR1B = SYSTEM_TIMER_CONTROL_B_SENTINEL;
    TCNT1 = SYSTEM_TIMER_COUNTER_SENTINEL;
    OCR1A = SYSTEM_TIMER_COMPARE_SENTINEL;
    interrupts_enabled = true;
    device_present = true;
    recovery_fault = false;
    released = false;
    cpu_cycles = clock_phase;
    delay_calls = 0U;
    timer_interrupts = 0U;
    onewire_init();
    assert(TCCR0 == 0U && TIMSK == OTHER_TIMER_INTERRUPTS);
    assert(TIFR == (_BV(OCF0) | _BV(TOV0)));
    assert((DDRD & _BV(PD2)) == 0U);
    assert(delay_calls == 0U);
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Check Reset deadlines, repeated polling and final bus diagnostics.
 * @param[in] clock_phase Initial shared-prescaler phase.
 * @param[in] present Whether the device emits a presence pulse.
 * @param[in] fault Whether the line remains low after presence.
 */
static void test_reset(uint8_t clock_phase, bool present, bool fault) {
    uint32_t limit;
    uint32_t recovery_started;
    uint8_t counter;
    EResult_t expected_result = eResultNotFound;

    if (fault) {
        expected_result = eResultBus;
    }
    else if (present) {
        expected_result = eResultOk;
    }
    prepare(clock_phase);
    device_present = present;
    recovery_fault = fault;
    pulse_started = cpu_cycles;
    assert(onewire_reset() == eResultBusy);
    assert(delay_calls == 0U);
    assert((DDRD & _BV(PD2)) != 0U);
    assert(TCCR0 == TIMER_MODE && OCR0 == RESET_LOW_COMPARE && TCNT0 == 0U);
    assert(TIFR == _BV(OCF0));
    advance_cycles(1U);
    assert((PIND & _BV(PD2)) == 0U);
    for (limit = 0U; limit < POLL_CYCLE_LIMIT && delay_calls == 0U; limit++) {
        counter = TCNT0;
        assert(onewire_reset() == eResultBusy);
        assert(TCNT0 == counter);
        advance_cycles(1U);
    }
    assert(delay_calls == 1U && timer_interrupts == 1U);
    assert((uint64_t)pulse_cycles * MICROSECONDS_PER_SECOND >=
           (uint64_t)F_CPU * RESET_LOW_MIN_US);
    assert((uint64_t)pulse_cycles * MICROSECONDS_PER_SECOND <
           (uint64_t)F_CPU * RESET_LOW_MAX_US);
    assert((DDRD & _BV(PD2)) == 0U);
    assert(TCCR0 == TIMER_MODE && OCR0 == RESET_RECOVERY_COMPARE);
    recovery_started = cpu_cycles;
    for (limit = 0U; limit < POLL_CYCLE_LIMIT && TCCR0 != 0U; limit++) {
        counter = TCNT0;
        assert(onewire_reset() == eResultBusy);
        assert(TCNT0 == counter);
        advance_cycles(1U);
    }
    assert(TCCR0 == 0U && (TIMSK & _BV(OCIE0)) == 0U);
    assert(timer_interrupts == RESET_INTERRUPT_COUNT && delay_calls == 1U);
    assert((uint64_t)(cpu_cycles - recovery_started) * MICROSECONDS_PER_SECOND >=
           (uint64_t)F_CPU * RESET_RECOVERY_MIN_US);
    advance_cycles(F_CPU / MILLISECONDS_PER_SECOND * POST_RESET_WAIT_MS);
    assert(timer_interrupts == RESET_INTERRUPT_COUNT && delay_calls == 1U);
    assert(onewire_reset() == expected_result);
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/** @brief Check idle-low rejection, retry, cancellation and IRQ restoration. */
static void test_cancel_and_fault(void) {
    uint32_t limit;
    unsigned int previous_interrupts;
    unsigned int previous_delays;

    prepare(CANCEL_CLOCK_PHASE);
    PIND &= (uint8_t)~_BV(PD2);
    assert(onewire_reset() == eResultBus);
    assert(TCCR0 == 0U && (DDRD & _BV(PD2)) == 0U);
    assert(delay_calls == 0U && timer_interrupts == 0U);
    PIND |= _BV(PD2);
    assert(onewire_reset() == eResultBusy);
    advance_cycles(CANCEL_LOW_CYCLES);
    onewire_init();
    assert((DDRD & _BV(PD2)) == 0U && TCCR0 == 0U);
    advance_cycles(F_CPU / MILLISECONDS_PER_SECOND * CANCEL_WAIT_MS);
    assert(delay_calls == 0U && timer_interrupts == 0U);

    released = false;
    pulse_started = cpu_cycles;
    assert(onewire_reset() == eResultBusy);
    for (limit = 0U; limit < POLL_CYCLE_LIMIT && delay_calls == 0U; limit++) {
        advance_cycles(1U);
    }
    assert(delay_calls == 1U);
    previous_interrupts = timer_interrupts;
    previous_delays = delay_calls;
    onewire_init();
    advance_cycles(F_CPU / MILLISECONDS_PER_SECOND * CANCEL_WAIT_MS);
    assert(TCCR0 == 0U && (TIMSK & _BV(OCIE0)) == 0U);
    assert(timer_interrupts == previous_interrupts);
    assert(delay_calls == previous_delays);
    interrupts_enabled = false;
    onewire_init();
    assert(!interrupts_enabled);
    assert(onewire_reset() == eResultBusy);
    assert(!interrupts_enabled);
    onewire_init();
    interrupts_enabled = true;
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/** @brief Check that timer deadlines progress without foreground polling. */
static void test_unpolled_reset(void) {
    prepare(TIMER_CLOCK_DIVIDER - 1U);
    pulse_started = cpu_cycles;
    assert(onewire_reset() == eResultBusy);
    advance_cycles(F_CPU / MILLISECONDS_PER_SECOND * UNPOLLED_WAIT_MS);
    assert(timer_interrupts == RESET_INTERRUPT_COUNT && delay_calls == 1U);
    assert(TCCR0 == 0U && (DDRD & _BV(PD2)) == 0U);
    assert(onewire_reset() == eResultOk);
    released = false;
    assert(onewire_reset() == eResultBusy);
    assert(TCCR0 == TIMER_MODE && (DDRD & _BV(PD2)) != 0U);
    onewire_init();
    check_isolation();
}

/********************** Application Programming Interface *********************/

uint8_t test_atomic_enter(void) {
    uint8_t ret_val = interrupts_enabled;

    interrupts_enabled = false;
    return ret_val;
}
/*----------------------------------------------------------------------------*/

void test_atomic_restore(uint8_t state) {
    interrupts_enabled = state != 0U;
}
/*----------------------------------------------------------------------------*/

void _delay_us(double us) {
    assert(us == PRESENCE_SAMPLE_US);
    assert(!interrupts_enabled && TCCR0 == 0U);
    assert((DDRD & _BV(PD2)) == 0U);
    delay_calls++;
    pulse_cycles = cpu_cycles - pulse_started;
    released = true;
    presence_end = cpu_cycles +
        (uint32_t)((uint64_t)F_CPU * PRESENCE_END_US / MICROSECONDS_PER_SECOND);
    PIND = device_present ? (uint8_t)(PIND & (uint8_t)~_BV(PD2)) :
        (uint8_t)(PIND | _BV(PD2));
    advance_cycles(
        (uint32_t)(((uint64_t)F_CPU * PRESENCE_SAMPLE_US +
                    MICROSECONDS_PER_SECOND - 1U) / MICROSECONDS_PER_SECOND)
    );
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Run Timer0 Reset timing, cancellation and isolation tests.
 * @returns Zero after all assertions pass.
 */
int main(void) {
    uint8_t phase;

    for (phase = 0U; phase < TIMER_CLOCK_DIVIDER; phase++) {
        test_reset(phase, true, false);
        test_reset(phase, false, false);
        test_reset(phase, true, true);
    }
    test_cancel_and_fault();
    test_unpolled_reset();
    puts("Timer0 Reset deadlines, GPIO isolation and non-blocking tests passed");
    return 0;
}
/******************************************************************************/
