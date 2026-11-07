/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    io.h
 * @version 0.1.9
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 * @brief Host-only register declarations for software 1-Wire timer tests.
 */

#ifndef TEST_AVR_IO_H
#define TEST_AVR_IO_H

#ifdef __AVR__
#error "Host test stubs must not be used in AVR builds"
#endif /* __AVR__ */

/******************************** Included files ******************************/
#include <stdint.h>

/********************************* Definitions ********************************/

/**
 * @def _BV
 * @brief Form a register bit mask.
 * @param[in] bit Register bit position.
 */
#define _BV(bit) (1U << (bit))

/**
 * @def PD2
 * @brief Port D bit position of the direct 1-Wire pin.
 */
#define PD2   2

/**
 * @def WGM01
 * @brief Timer0 waveform-generation bit selecting CTC mode.
 */
#define WGM01 3

/**
 * @def CS01
 * @brief Timer0 clock-selection bit 1.
 */
#define CS01  1

/**
 * @def CS00
 * @brief Timer0 clock-selection bit 0.
 */
#define CS00  0

/**
 * @def OCIE0
 * @brief Timer0 compare-interrupt enable bit.
 */
#define OCIE0 1

/**
 * @def TOIE0
 * @brief Timer0 overflow-interrupt enable bit.
 */
#define TOIE0 0

/**
 * @def OCF0
 * @brief Timer0 compare flag bit, cleared by writing one.
 */
#define OCF0  1

/**
 * @def TOV0
 * @brief Timer0 overflow flag bit, cleared by writing one.
 */
#define TOV0  0

/****************************** Module variables ******************************/

/** @brief Modeled Port D direction register. */
extern volatile uint8_t DDRD;
/** @brief Modeled Port D output latch. */
extern volatile uint8_t PORTD;
/** @brief Modeled Port D input levels. */
extern volatile uint8_t PIND;
/** @brief Modeled Timer0 control register. */
extern volatile uint8_t TCCR0;
/** @brief Modeled Timer0 counter. */
extern volatile uint8_t TCNT0;
/** @brief Modeled Timer0 compare register. */
extern volatile uint8_t OCR0;
/** @brief Modeled shared timer-interrupt enable register. */
extern volatile uint8_t TIMSK;
/** @brief Last write to the modeled timer-interrupt flag register. */
extern volatile uint8_t TIFR;
/** @brief Modeled shared prescaler-control register. */
extern volatile uint8_t SFIOR;
/** @brief Modeled Timer1 control register A. */
extern volatile uint8_t TCCR1A;
/** @brief Modeled Timer1 control register B. */
extern volatile uint8_t TCCR1B;
/** @brief Modeled Timer1 counter. */
extern volatile uint16_t TCNT1;
/** @brief Modeled Timer1 compare register A. */
extern volatile uint16_t OCR1A;

/******************************************************************************/
#endif // !TEST_AVR_IO_H
