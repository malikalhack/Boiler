/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    onewire.h
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */
#ifndef ONEWIRE_H
#define ONEWIRE_H

/******************************** Included files ******************************/
#include <stdbool.h>

#include "errors.h"
/********************** Application Programming Interface *********************/

/**
 * @brief Initialize the software open-drain bus with PD2 released.
 * @note An external pull-up and separate sensor VDD are required.
 */
void onewire_init(void);

/**
 * @brief Perform a standard-speed reset and detect the presence pulse.
 * @retval eResultOk Presence detected and the bus recovered high.
 * @retval eResultNotFound Bus is idle high without a presence pulse.
 * @retval eResultBus Bus is low before reset or after recovery.
 * @note One call occupies approximately 960 us of cooperative execution.
 */
EResult_t onewire_reset(void);

/**
 * @brief Write one standard-speed bit to the bus.
 * @param[in] value True for Write 1; false for Write 0.
 * @note PD2 is never driven high; critical timing preserves interrupt state.
 */
void onewire_write_bit(bool value);

/**
 * @brief Read one standard-speed bit from the bus.
 * @returns True for a high sample; false for a low sample.
 * @note Sampling occurs within 15 us; interrupt state is preserved.
 */
bool onewire_read_bit(void);

/******************************************************************************/
#endif // !ONEWIRE_H
