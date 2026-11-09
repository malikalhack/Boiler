/**
 * @copyright 2021 Anthony Chernov
 * Licensed under the MIT License; see LICENSE in the project root.
 * @file    acrosched_port.h
 * @version 2.7.1
 * @authors Anton Chernov
 * @date    2026-05-14
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef ACROSCHED_PORT_H
#define ACROSCHED_PORT_H

/******************************** Included files ******************************/
#include <avr/io.h>
#include <avr/interrupt.h>

/********************************* Definitions ********************************/

/*
 * ATmega timers are 8 or 16 bits wide. Default uint16_t from
 * acrosched_config.h is appropriate - no override needed.
 */

/**
 * @def ACROSCHED_DECLARE_CRITICAL
 * @brief Local storage for the saved status register (SREG, holds the I-flag).
 */
#define ACROSCHED_DECLARE_CRITICAL()  uint8_t _acroSreg

/**
 * @def ACROSCHED_ENTER_CRITICAL
 * @brief Save SREG (global interrupt flag) and disable interrupts (AVR cli).
 * @details Nesting-safe: ACROSCHED_EXIT_CRITICAL() restores the saved SREG,
 * so interrupts that were already disabled on entry stay disabled on exit.
 */
#define ACROSCHED_ENTER_CRITICAL()  \
    do {                            \
        _acroSreg = SREG;           \
        cli();                      \
    } while (0)

/**
 * @def ACROSCHED_EXIT_CRITICAL
 * @brief Restore the SREG (and thus the I-flag) saved by
 * ACROSCHED_ENTER_CRITICAL().
 */
#define ACROSCHED_EXIT_CRITICAL()   (SREG = _acroSreg)

/**
 * @def ACROSCHED_WAIT_FOR_EVENT
 * @brief Enter sleep until the next interrupt (AVR SLEEP instruction).
 * @details Used by the optional idle-hook module (ACROSCHED_USE_IDLE_HOOK) to
 * stop the core when no task ran during a dispatcher iteration. The SLEEP
 * instruction only acts when a sleep mode has been selected and the sleep
 * enable (SE) bit is set; the application configures the desired mode (for
 * example with set_sleep_mode() / sleep_enable() from <avr/sleep.h>). With SE
 * clear the instruction is a harmless no-op. Interrupts must be enabled so a
 * source (such as the timer tick) can wake the core.
 */
#define ACROSCHED_WAIT_FOR_EVENT()  __asm__ __volatile__ ("sleep" : : : "memory")

/******************************************************************************/
#endif // !ACROSCHED_PORT_H
