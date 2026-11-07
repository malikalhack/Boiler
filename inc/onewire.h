/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    onewire.h
 * @version 0.1.10
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
 * @brief Release PD2 and initialize dedicated Timer0 for software Reset.
 * @note An external pull-up and separate sensor VDD are required.
 * Cancels any pending Reset. Timer1 and its shared prescaler are unchanged.
 */
void onewire_init(void);

/**
 * @brief Start or poll a timer-driven Reset and presence detection.
 * @retval eResultBusy Reset was started or its timed phases are pending.
 * @retval eResultOk Presence detected and the bus recovered high.
 * @retval eResultNotFound Bus is idle high without a presence pulse.
 * @retval eResultBus Bus is low before reset or after recovery.
 * @note Call after initialization, from cooperative tasks. Interrupts must
 * run while Reset is pending. Polling does not restart the pulse.
 * Timer0 ends the long phases independently of scheduler calls; its ISR
 * retains the 70 us presence sample wait. A completed result is consumed once;
 * the following call starts another Reset.
 */
EResult_t onewire_reset(void);

/**
 * @brief Write one standard-speed bit to the bus.
 * @param[in] value True for Write 1; false for Write 0.
 * @note PD2 is never driven high; critical timing preserves interrupt state.
 * Call only when no Reset is pending and its result has been consumed.
 */
void onewire_write_bit(bool value);

/**
 * @brief Read one standard-speed bit from the bus.
 * @returns True for a high sample; false for a low sample.
 * @note Sampling occurs within 15 us; interrupt state is preserved.
 * Call only when no Reset is pending and its result has been consumed.
 */
bool onewire_read_bit(void);

/******************************************************************************/
#endif // !ONEWIRE_H
