/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    atomic.h
 * @version 0.1.9
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 * @brief Host-only interrupt-state model for normal-exit atomic blocks.
 */

#ifndef TEST_UTIL_ATOMIC_H
#define TEST_UTIL_ATOMIC_H

#ifdef __AVR__
#error "Host test stubs must not be used in AVR builds"
#endif /* __AVR__ */

/******************************** Included files ******************************/
#include <stdint.h>

/********************************* Definitions ********************************/

/**
 * @def ATOMIC_RESTORESTATE
 * @brief Select interrupt-state restoration in the host atomic-block model.
 */
#define ATOMIC_RESTORESTATE 0U

/**
 * @def ATOMIC_BLOCK
 * @brief Mask interrupts and restore their state after a normal block exit.
 * @param[in] type Restoration policy; only ATOMIC_RESTORESTATE is supported.
 * @note This test model does not support return or break inside the block.
 */
#define ATOMIC_BLOCK(type) \
    for (uint8_t saved_state = test_atomic_enter(), run = 1U; run != 0U; \
         test_atomic_restore(saved_state), run = 0U)

/********************** Application Programming Interface *********************/

/**
 * @brief Mask modeled interrupts.
 * @returns Previous interrupt-enabled state.
 */
uint8_t test_atomic_enter(void);
/*----------------------------------------------------------------------------*/

/**
 * @brief Restore modeled interrupts.
 * @param[in] state Previously saved interrupt-enabled state.
 */
void test_atomic_restore(uint8_t state);

/******************************************************************************/
#endif // !TEST_UTIL_ATOMIC_H
