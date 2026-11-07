/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    reset_timer_test.c
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 * @brief Execute the production 1-Wire driver with independent bus clocks.
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
 * @def RESET_CLOCK_DIVIDER
 * @brief Independent model of the free-running Reset prescaler.
 */
#define RESET_CLOCK_DIVIDER 64U

/**
 * @def SLOT_CLOCK_DIVIDER
 * @brief Independent model of the finer slot and presence prescaler.
 */
#define SLOT_CLOCK_DIVIDER 8U

/**
 * @def RESET_MODE
 * @brief Timer0 CTC, /64 clock and disconnected OC0.
 */
#define RESET_MODE (_BV(WGM01) | _BV(CS01) | _BV(CS00))

/**
 * @def SLOT_MODE
 * @brief Timer0 CTC, /8 clock and disconnected OC0.
 */
#define SLOT_MODE (_BV(WGM01) | _BV(CS01))

/**
 * @def OTHER_TIMER_INTERRUPTS
 * @brief Interrupt-enable bits outside the Timer0 driver's ownership.
 */
#define OTHER_TIMER_INTERRUPTS ((uint8_t)~(_BV(OCIE0) | _BV(TOIE0)))

/**
 * @def BYTE_BITS
 * @brief Number of slots in a least-significant-bit-first byte.
 */
#define BYTE_BITS 8U

/**
 * @enum EModelBus
 * @brief External device behavior for Reset, data and search-slot tests.
 */
typedef enum EModelBus {
    eModelReset,   /**< Emit configurable presence after Reset release. */
    eModelWrite,   /**< Decode master low pulses into written bits. */
    eModelRead,    /**< Supply a configurable least-significant-bit pattern. */
    eModelTriplet  /**< Supply a search pair and decode the selected branch. */
} EModelBus_t;

/** @brief Microsecond conversion factor for independent cycle assertions. */
static const uint32_t kMicrosecondsPerSecond = 1000000UL;
/** @brief Unrelated Port D direction sentinel. */
static const uint8_t kDirectionSentinel = 0xF0U;
/** @brief Unrelated Port D output sentinel. */
static const uint8_t kOutputSentinel = 0xB0U;
/** @brief Shared prescaler-control sentinel. */
static const uint8_t kPrescalerSentinel = 0xA0U;
/** @brief Timer1 control register A sentinel. */
static const uint8_t kTimer1ControlA = 0x12U;
/** @brief Timer1 control register B sentinel. */
static const uint8_t kTimer1ControlB = 0x34U;
/** @brief Timer1 counter sentinel. */
static const uint16_t kTimer1Counter = 0x1234U;
/** @brief Timer1 compare register A sentinel. */
static const uint16_t kTimer1Compare = 0x5678U;
/** @brief Read-output sentinel retained until successful completion. */
static const uint8_t kReadSentinel = 0xA5U;
/** @brief Minimum Reset low time in microseconds. */
static const uint16_t kResetLowUs = 480U;
/** @brief Upper bound including modeled interrupt latency in microseconds. */
static const uint16_t kResetLowLimitUs = 520U;
/** @brief Minimum recovery after presence sampling in microseconds. */
static const uint16_t kResetRecoveryUs = 410U;
/** @brief Independently expected long-clock Reset compare. */
static const uint8_t kResetCompare = 112U;
/** @brief Independently expected long-clock recovery compare. */
static const uint8_t kRecoveryCompare = 96U;
/** @brief Latest permitted presence start in microseconds. */
static const uint8_t kPresenceLatestUs = 60U;
/** @brief Earliest permitted presence start in microseconds. */
static const uint8_t kPresenceEarliestUs = 15U;
/** @brief Minimum permitted device presence duration in microseconds. */
static const uint8_t kPresenceWidthUs = 60U;
/** @brief End of the guaranteed presence overlap in microseconds. */
static const uint8_t kPresenceOverlapEndUs = 75U;
/** @brief Minimum complete slot duration in microseconds. */
static const uint8_t kSlotUs = 70U;
/** @brief Minimum Write 0 low duration in microseconds. */
static const uint8_t kWriteZeroUs = 60U;
/** @brief Maximum standard-speed Write 0 low duration in microseconds. */
static const uint8_t kWriteZeroLimitUs = 120U;
/** @brief Maximum read-sample or Write 1 low duration in microseconds. */
static const uint8_t kShortLimitUs = 15U;
/** @brief Retained Write 1 software delay in microseconds. */
static const uint8_t kWriteOneUs = 6U;
/** @brief Retained read low delay in microseconds. */
static const uint8_t kReadLowUs = 3U;
/** @brief Retained software wait after read release in microseconds. */
static const uint8_t kReadSampleUs = 8U;
/** @brief Maximum combined software wait in a foreground transfer start. */
static const uint8_t kForegroundWaitUs = 11U;
/** @brief Maximum slots in a complete ROM-search triplet. */
static const uint8_t kSearchSlots = 3U;
/** @brief Timer interrupts required by the three Reset phases. */
static const uint8_t kResetInterrupts = 3U;
/** @brief Maximum modeled cycles before a missing deadline fails the test. */
static const uint32_t kWaitCycles = 50000UL;
/** @brief Observation interval after completion or cancellation in cycles. */
static const uint32_t kObservationCycles = F_CPU / 1000U * 3U;
/** @brief Vector entry and prologue through the early PIND sample in cycles. */
static const uint8_t kEntryCycles = 40U;
/** @brief Reset release to restarted clock, checked against AVR disassembly. */
static const uint8_t kPresenceArmCycles = 23U;
/** @brief Clock-start to ISR-return tail, including ret, branch and epilogue. */
static const uint8_t kExitCycles = 41U;
/** @brief Poll masking, Timer1 ISR and instruction completion cycle budget. */
static const uint8_t kBlockingCycles = 86U;

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

static EModelBus_t model_bus;
static bool interrupts_enabled;
static bool compare_pending;
static bool device_present;
static bool recovery_fault;
static bool idle_fault;
static bool master_low;
static bool released;
static bool device_low;
static uint8_t presence_start_us;
static uint8_t blocking_cycles;
static uint8_t model_data;
static uint8_t written_data;
static uint8_t written_bits;
static uint8_t slots;
static uint32_t cpu_cycles;
static uint32_t low_started;
static uint32_t slot_started;
static uint32_t reset_low_cycles;
static uint32_t released_at;
static uint32_t presence_on;
static uint32_t presence_off;
static uint32_t presence_sample_cycles;
static uint32_t recovery_started;
static uint32_t recovery_cycles;
static uint32_t device_release;
static unsigned int delay_calls;
static unsigned int timer_interrupts;

/***************************** Private prototypes *****************************/

static void advance_cycles(uint32_t count);

/****************************** Private functions *****************************/

/**
 * @brief Convert a protocol interval to a rounded-up independent cycle count.
 * @param[in] us Interval in microseconds.
 * @returns Number of CPU cycles covering the requested interval.
 */
static uint32_t cycles_for(uint16_t us) {
    return (uint32_t)(((uint64_t)F_CPU * us + kMicrosecondsPerSecond - 1U) /
                      kMicrosecondsPerSecond);
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Check actual cycles against independent physical interval bounds.
 * @param[in] actual Observed CPU cycles.
 * @param[in] minimum Inclusive lower bound in microseconds.
 * @param[in] maximum Exclusive upper bound in microseconds.
 */
static void check_interval(uint32_t actual, uint16_t minimum, uint16_t maximum) {
    assert((uint64_t)actual * kMicrosecondsPerSecond >=
           (uint64_t)F_CPU * minimum);
    assert((uint64_t)actual * kMicrosecondsPerSecond <
           (uint64_t)F_CPU * maximum);
}
/*----------------------------------------------------------------------------*/

/** @brief Record master GPIO edges and update the independent device response. */
static void update_bus(void) {
    bool driving = (DDRD & _BV(PD2)) != 0U;
    bool writing = model_bus == eModelWrite;
    uint32_t low_cycles;

    if (model_bus == eModelTriplet) {
        writing = slots == kSearchSlots;
    }
    if (driving && !master_low) {
        low_started = cpu_cycles;
        if (model_bus != eModelReset) {
            if (slots != 0U) {
                assert(cpu_cycles - slot_started >= cycles_for(kSlotUs));
            }
            slot_started = cpu_cycles;
            slots++;
        }
    }
    else if (!driving && master_low) {
        low_cycles = cpu_cycles - low_started;
        if (model_bus == eModelReset) {
            reset_low_cycles = low_cycles;
            released = true;
            released_at = cpu_cycles;
            presence_on = cpu_cycles + cycles_for(presence_start_us);
            presence_off = presence_on + cycles_for(kPresenceWidthUs);
        }
        else if (writing) {
            if (low_cycles >= cycles_for(kWriteZeroUs)) {
                check_interval(low_cycles, kWriteZeroUs, kWriteZeroLimitUs);
            }
            else {
                check_interval(low_cycles, kWriteOneUs, kShortLimitUs);
                written_data |= 1U << written_bits;
            }
            written_bits++;
            device_low = false;
        }
        else {
            check_interval(low_cycles, kReadLowUs, kShortLimitUs);
            device_low = (model_data & 1U << (slots - 1U)) == 0U;
            device_release = slot_started + cycles_for(kWriteZeroUs);
        }
    }
    master_low = driving;
    if (driving || idle_fault) {
        PIND &= (uint8_t)~_BV(PD2);
    }
    else if (model_bus == eModelReset && released) {
        bool presence = device_present && cpu_cycles >= presence_on &&
            cpu_cycles < presence_off;

        if (recovery_fault || presence) {
            PIND &= (uint8_t)~_BV(PD2);
        }
        else {
            PIND |= _BV(PD2);
        }
    }
    else if (device_low && cpu_cycles < device_release) {
        PIND &= (uint8_t)~_BV(PD2);
    }
    else {
        PIND |= _BV(PD2);
    }
}
/*----------------------------------------------------------------------------*/

/** @brief Clock the independent CTC counter without resetting clock phase. */
static void clock_timer(void) {
    uint8_t divider;

    if (TCCR0 != RESET_MODE && TCCR0 != SLOT_MODE) {
        return;
    }
    divider = TCCR0 == RESET_MODE ? RESET_CLOCK_DIVIDER : SLOT_CLOCK_DIVIDER;
    if (cpu_cycles % divider == 0U) {
        if (TCNT0 == OCR0) {
            TCNT0 = 0U;
            compare_pending = true;
        }
        else {
            TCNT0++;
        }
    }
}
/*----------------------------------------------------------------------------*/

/** @brief Dispatch Timer0 with bounded entry, blocking and epilogue overhead. */
static void dispatch_compare(void) {
    bool was_released = released;
    uint8_t clock;

    compare_pending = false;
    interrupts_enabled = false;
    advance_cycles(kEntryCycles + blocking_cycles);
    if (model_bus == eModelReset && released && TCCR0 == SLOT_MODE) {
        presence_sample_cycles = cpu_cycles - released_at;
    }
    timer_interrupts++;
    TIMER0_COMP_vect();
    update_bus();
    if (model_bus == eModelReset && !was_released && released) {
        clock = TCCR0;
        TCCR0 = 0U;
        advance_cycles(kPresenceArmCycles);
        TCCR0 = clock;
    }
    if (model_bus == eModelReset && TCCR0 == RESET_MODE && released) {
        recovery_started = cpu_cycles;
    }
    if (model_bus == eModelReset && TCCR0 == 0U && released) {
        recovery_cycles = cpu_cycles - recovery_started;
    }
    advance_cycles(kExitCycles);
    interrupts_enabled = true;
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Advance free-running clocks and dispatch unmasked compare events.
 * @param[in] count Number of CPU cycles to advance.
 * @note CTC compares the current count before clearing or incrementing it.
 */
static void advance_cycles(uint32_t count) {
    update_bus();
    while (count-- != 0U) {
        cpu_cycles++;
        update_bus();
        clock_timer();
        if ((TIMSK & _BV(OCIE0)) == 0U) {
            compare_pending = false;
        }
        if (compare_pending && interrupts_enabled) {
            dispatch_compare();
        }
    }
}
/*----------------------------------------------------------------------------*/

/** @brief Check GPIO ownership and shared-prescaler/system-timer isolation. */
static void check_isolation(void) {
    assert((DDRD & (uint8_t)~_BV(PD2)) == kDirectionSentinel);
    assert(PORTD == kOutputSentinel);
    assert((TIMSK & OTHER_TIMER_INTERRUPTS) == OTHER_TIMER_INTERRUPTS);
    assert(SFIOR == kPrescalerSentinel);
    assert(TCCR1A == kTimer1ControlA && TCCR1B == kTimer1ControlB);
    assert(TCNT1 == kTimer1Counter && OCR1A == kTimer1Compare);
    assert(interrupts_enabled);
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Initialize independent device and register state at a clock phase.
 * @param[in] clock_phase Shared-prescaler phase in CPU cycles.
 * @param[in] bus External device behavior for the operation.
 * @param[in] blocking Additional interrupt-entry blocking cycles.
 */
static void prepare(uint8_t clock_phase, EModelBus_t bus, uint8_t blocking) {
    DDRD = kDirectionSentinel | _BV(PD2);
    PORTD = kOutputSentinel | _BV(PD2);
    PIND = UINT8_MAX;
    TIMSK = UINT8_MAX;
    TCCR0 = UINT8_MAX;
    SFIOR = kPrescalerSentinel;
    TCCR1A = kTimer1ControlA;
    TCCR1B = kTimer1ControlB;
    TCNT1 = kTimer1Counter;
    OCR1A = kTimer1Compare;
    interrupts_enabled = true;
    compare_pending = false;
    device_present = true;
    recovery_fault = false;
    idle_fault = false;
    master_low = false;
    released = false;
    device_low = false;
    presence_start_us = kPresenceEarliestUs;
    blocking_cycles = blocking;
    model_bus = bus;
    model_data = 0U;
    written_data = 0U;
    written_bits = 0U;
    slots = 0U;
    cpu_cycles = clock_phase;
    delay_calls = 0U;
    timer_interrupts = 0U;
    onewire_init();
    assert(TCCR0 == 0U && TIMSK == OTHER_TIMER_INTERRUPTS);
    assert(TIFR == (_BV(OCF0) | _BV(TOV0)));
    assert((DDRD & _BV(PD2)) == 0U);
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/** @brief Complete an operation without any foreground polling. */
static void wait_timer(void) {
    uint32_t limit;

    for (limit = 0U; limit < kWaitCycles && TCCR0 != 0U; limit++) {
        advance_cycles(1U);
    }
    assert(TCCR0 == 0U && (TIMSK & _BV(OCIE0)) == 0U);
    assert(!master_low);
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Check Reset timing, presence overlap, polling and recovery diagnostics.
 * @param[in] phase Initial shared-prescaler phase.
 * @param[in] blocking Additional interrupt blocking budget.
 * @param[in] start Presence start after release in microseconds.
 * @param[in] present Whether the device emits presence.
 * @param[in] fault Whether recovery remains stuck low.
 */
static void test_reset(
    uint8_t phase, uint8_t blocking, uint8_t start, bool present, bool fault
) {
    uint8_t counter;
    uint32_t limit;
    EResult_t expected = eResultNotFound;

    prepare(phase, eModelReset, blocking);
    presence_start_us = start;
    device_present = present;
    recovery_fault = fault;
    if (fault) {
        expected = eResultBus;
    }
    else if (present) {
        expected = eResultOk;
    }
    assert(onewire_reset() == eResultBusy);
    assert(TCCR0 == RESET_MODE && OCR0 == kResetCompare && TCNT0 == 0U);
    assert(TIFR == _BV(OCF0));
    advance_cycles(1U);
    assert((PIND & _BV(PD2)) == 0U);
    for (limit = 0U; limit < kWaitCycles && TCCR0 != 0U; limit++) {
        counter = TCNT0;
        assert(onewire_reset() == eResultBusy);
        assert(onewire_write_bit(true) == eResultBusy);
        assert(TCNT0 == counter);
        advance_cycles(1U);
    }
    assert(TCCR0 == 0U && timer_interrupts == kResetInterrupts);
    assert(OCR0 == kRecoveryCompare && delay_calls == 0U);
    check_interval(reset_low_cycles, kResetLowUs, kResetLowLimitUs);
    check_interval(presence_sample_cycles, kPresenceLatestUs,
                   kPresenceOverlapEndUs);
    assert(recovery_cycles >= cycles_for(kResetRecoveryUs));
    advance_cycles(kObservationCycles);
    assert(timer_interrupts == kResetInterrupts);
    assert(onewire_reset() == expected);
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Check all byte values, LSB order, bounded start and autonomous slots.
 * @param[in] phase Initial shared-prescaler phase.
 * @param[in] value Byte to send and independently receive.
 */
static void test_byte(uint8_t phase, uint8_t value) {
    uint32_t started;
    uint8_t result = kReadSentinel;
    uint8_t counter;
    uint8_t blocking = (phase & 1U) != 0U ? kBlockingCycles : 0U;

    prepare(phase, eModelWrite, blocking);
    started = cpu_cycles;
    assert(onewire_write_byte(value) == eResultBusy);
    assert(cpu_cycles - started <= cycles_for(kForegroundWaitUs));
    assert(slots <= 1U);
    counter = TCNT0;
    assert(onewire_write_byte(value ^ UINT8_MAX) == eResultBusy);
    assert(onewire_read_byte(&result) == eResultBusy);
    assert(result == kReadSentinel && TCNT0 == counter);
    wait_timer();
    assert(written_bits == BYTE_BITS && written_data == value);
    assert(onewire_write_byte(value ^ UINT8_MAX) == eResultOk);
    check_isolation();

    prepare(phase, eModelRead, blocking);
    model_data = value;
    started = cpu_cycles;
    assert(onewire_read_byte(&result) == eResultBusy);
    assert(cpu_cycles - started <= cycles_for(kForegroundWaitUs));
    assert(result == kReadSentinel && slots == 1U);
    wait_timer();
    assert(slots == BYTE_BITS && written_bits == 0U);
    assert(onewire_read_byte(&result) == eResultOk && result == value);
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Check single-bit reads/writes and complete search branch decisions.
 * @param[in] phase Initial shared-prescaler phase.
 * @param[in] pair Independent ID/complement input pair.
 * @param[in] preferred Preferred collision branch.
 */
static void test_bits(uint8_t phase, uint8_t pair, bool preferred) {
    bool bit = !preferred;
    bool direction = pair == ONEWIRE_SEARCH_ID;
    uint8_t result = kReadSentinel;

    prepare(phase, eModelWrite, kBlockingCycles);
    assert(onewire_write_bit(preferred) == eResultBusy);
    wait_timer();
    assert(written_bits == 1U && written_data == preferred);
    assert(onewire_write_bit(!preferred) == eResultOk);

    prepare(phase, eModelRead, kBlockingCycles);
    model_data = preferred;
    assert(onewire_read_bit(&bit) == eResultBusy && bit == !preferred);
    wait_timer();
    assert(onewire_read_bit(&bit) == eResultOk && bit == preferred);

    prepare(phase, eModelTriplet, kBlockingCycles);
    model_data = pair;
    assert(onewire_triplet(preferred, &result) == eResultBusy);
    assert(result == kReadSentinel);
    wait_timer();
    if (pair == (ONEWIRE_SEARCH_ID | ONEWIRE_SEARCH_COMPLEMENT)) {
        assert(slots == kSearchSlots - 1U && written_bits == 0U);
        assert(onewire_triplet(!preferred, &result) == eResultBus);
        assert(result == kReadSentinel);
    }
    else {
        if (pair == 0U) {
            direction = preferred;
        }
        assert(slots == kSearchSlots && written_bits == 1U);
        assert(written_data == direction);
        assert(onewire_triplet(!preferred, &result) == eResultOk);
        assert(result == (pair | (direction ? ONEWIRE_SEARCH_DIRECTION : 0U)));
    }
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/** @brief Check idle/recovery faults, invalid outputs and IRQ restoration. */
static void test_faults(void) {
    uint8_t value = kReadSentinel;
    bool bit = true;

    prepare(0U, eModelRead, 0U);
    assert(onewire_read_bit(NULL) == eResultInvalidArgument);
    assert(onewire_read_byte(NULL) == eResultInvalidArgument);
    assert(onewire_triplet(false, NULL) == eResultInvalidArgument);
    assert(TCCR0 == 0U && delay_calls == 0U);
    idle_fault = true;
    update_bus();
    assert(onewire_reset() == eResultBus);
    assert(onewire_read_bit(&bit) == eResultBus && bit);
    assert(onewire_read_byte(&value) == eResultBus && value == kReadSentinel);
    assert(onewire_write_byte(0U) == eResultBus && TCCR0 == 0U);
    idle_fault = false;
    update_bus();
    assert(onewire_read_byte(&value) == eResultBusy);
    idle_fault = true;
    wait_timer();
    assert(slots == 1U);
    assert(onewire_read_byte(&value) == eResultBus && value == kReadSentinel);
    interrupts_enabled = false;
    onewire_init();
    assert(!interrupts_enabled);
    idle_fault = false;
    update_bus();
    assert(onewire_reset() == eResultBusy && !interrupts_enabled);
    onewire_init();
    interrupts_enabled = true;
    check_isolation();
}
/*----------------------------------------------------------------------------*/

/** @brief Cancel each Reset phase and preempt active or completed byte traffic. */
static void test_cancellation(void) {
    uint8_t phase;
    uint32_t limit;
    unsigned int previous_interrupts;

    for (phase = 0U; phase < kResetInterrupts; phase++) {
        prepare(phase, eModelReset, kBlockingCycles);
        assert(onewire_reset() == eResultBusy);
        for (limit = 0U;
             limit < kWaitCycles && timer_interrupts < phase; limit++) {
            advance_cycles(1U);
        }
        assert(timer_interrupts == phase);
        onewire_init();
        advance_cycles(kObservationCycles);
        assert(TCCR0 == 0U && timer_interrupts == phase && !master_low);
        check_isolation();
    }
    for (phase = 0U; phase < kResetInterrupts; phase++) {
        prepare(phase, eModelWrite, kBlockingCycles);
        assert(onewire_write_byte(0U) == eResultBusy);
        if (phase == 1U) {
            while (timer_interrupts == 0U && cpu_cycles < kWaitCycles) {
                advance_cycles(1U);
            }
            assert(timer_interrupts == 1U);
        }
        else if (phase != 0U) {
            wait_timer();
        }
        previous_interrupts = timer_interrupts;
        model_bus = eModelReset;
        low_started = cpu_cycles;
        assert(onewire_reset() == eResultBusy);
        assert(TCCR0 == RESET_MODE && OCR0 == kResetCompare);
        wait_timer();
        assert(timer_interrupts == previous_interrupts + kResetInterrupts);
        assert(onewire_reset() == eResultOk);
        onewire_init();
        check_isolation();
    }
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
    assert(!interrupts_enabled && TCCR0 == 0U);
    assert(us == kWriteOneUs || us == kReadLowUs || us == kReadSampleUs);
    delay_calls++;
    advance_cycles(cycles_for((uint16_t)us));
    if (us == kReadSampleUs) {
        check_interval(cpu_cycles - slot_started, kForegroundWaitUs,
                       kShortLimitUs);
    }
}
/*----------------------------------------------------------------------------*/

/**
 * @brief Run exhaustive timer-driven Reset, data and search-slot tests.
 * @returns Zero after all assertions pass.
 */
int main(void) {
    uint8_t phase;
    uint8_t pair;
    uint16_t value;

    for (phase = 0U; phase < RESET_CLOCK_DIVIDER; phase++) {
        test_reset(phase, 0U, kPresenceEarliestUs, true, false);
        test_reset(phase, kBlockingCycles, kPresenceEarliestUs, true, false);
        test_reset(phase, kBlockingCycles, kPresenceLatestUs, true, false);
        test_reset(phase, kBlockingCycles, kPresenceLatestUs, false, false);
        test_reset(phase, kBlockingCycles, kPresenceLatestUs, true, true);
        for (value = 0U; value <= UINT8_MAX; value++) {
            test_byte(phase, (uint8_t)value);
        }
        for (pair = 0U;
             pair <= (ONEWIRE_SEARCH_ID | ONEWIRE_SEARCH_COMPLEMENT); pair++) {
            test_bits(phase, pair, false);
            test_bits(phase, pair, true);
        }
    }
    test_faults();
    test_cancellation();
    puts("Timer0 Reset, autonomous slots/bytes and GPIO isolation tests passed");
    return 0;
}
/******************************************************************************/
