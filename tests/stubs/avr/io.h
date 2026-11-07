/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    io.h
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-11-03
 * @date    @showdate "%Y-%m-%d"
 * @brief Host-only include stub; tests mock BSP and scheduler functions.
 */

#ifndef TEST_AVR_IO_H
#define TEST_AVR_IO_H
#ifdef __AVR__
#error "Host test stubs must not be used in AVR builds"
#endif /* __AVR__ */
#endif // !TEST_AVR_IO_H
