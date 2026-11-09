/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    delay.h
 * @version 0.1.12
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 * @brief Host-only software-delay callback for timer and bus assertions.
 */

#ifndef TEST_UTIL_DELAY_H
#define TEST_UTIL_DELAY_H

#ifdef __AVR__
#error "Host test stubs must not be used in AVR builds"
#endif /* __AVR__ */

/********************** Application Programming Interface *********************/

/**
 * @brief Advance modeled clocks during the remaining software presence wait.
 * @param[in] us Requested delay in microseconds.
 */
void _delay_us(double us);

/******************************************************************************/
#endif // !TEST_UTIL_DELAY_H
