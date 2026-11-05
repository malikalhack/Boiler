/**
 * @file    interrupt.h
 * @version 0.1.2
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 * @brief Host-only include stub; no AVR interrupt operations are emulated.
 */

#ifndef TEST_AVR_INTERRUPT_H
#define TEST_AVR_INTERRUPT_H
#ifdef __AVR__
#error "Host test stubs must not be used in AVR builds"
#endif /* __AVR__ */
#endif // !TEST_AVR_INTERRUPT_H
