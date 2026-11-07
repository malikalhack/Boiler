/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    interrupt.h
 * @version 0.1.10
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 * @brief Host-only declaration of a manually dispatched interrupt handler.
 */

#ifndef TEST_AVR_INTERRUPT_H
#define TEST_AVR_INTERRUPT_H

#ifdef __AVR__
#error "Host test stubs must not be used in AVR builds"
#endif /* __AVR__ */

/********************************* Definitions ********************************/

/**
 * @def ISR
 * @brief Declare an interrupt handler callable by a host hardware model.
 * @param[in] vector Interrupt vector name.
 */
#define ISR(vector) void vector(void)

/********************** Application Programming Interface *********************/

/**
 * @brief Dispatch the production Timer0 compare handler in the host model.
 */
void TIMER0_COMP_vect(void);

/******************************************************************************/
#endif // !TEST_AVR_INTERRUPT_H
