/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    onewire.h
 * @version 0.1.12
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */
#ifndef ONEWIRE_H
#define ONEWIRE_H

/******************************** Included files ******************************/
#include <stdbool.h>
#include <stdint.h>

#include "errors.h"
/********************************* Definitions ********************************/

/**
 * @def ONEWIRE_SEARCH_ID
 * @brief Search triplet result bit containing the first bus read.
 */
#define ONEWIRE_SEARCH_ID         0x01U

/**
 * @def ONEWIRE_SEARCH_COMPLEMENT
 * @brief Search triplet result bit containing the complementary bus read.
 */
#define ONEWIRE_SEARCH_COMPLEMENT 0x02U

/**
 * @def ONEWIRE_SEARCH_DIRECTION
 * @brief Search triplet result bit containing the transmitted branch choice.
 */
#define ONEWIRE_SEARCH_DIRECTION  0x04U

/********************** Application Programming Interface *********************/

/**
 * @brief Release PD2 and initialize dedicated Timer0 for software 1-Wire.
 * @note An external pull-up and separate sensor VDD are required.
 * Cancels any operation or unconsumed result. Timer1 and its prescaler
 * are unchanged. Call before any other bus operation.
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
 * Timer0 handles presence and all long waits independently of scheduler
 * calls. A completed result is consumed once; the next call starts a Reset.
 * Reset preempts pending bit/byte/triplet operations or their results,
 * allowing an acquisition refresh to restart safely.
 * Revalidate the presence timing budget after CPU clock changes or adding
 * interrupt handlers or longer masked regions. The current task list is
 * set up at startup.
 */
EResult_t onewire_reset(void);

/**
 * @brief Start or poll one standard-speed write slot.
 * @param[in] value True for Write 1; false for Write 0.
 * @retval eResultBusy Write slot was started or is pending.
 * @retval eResultOk Write slot completed.
 * @retval eResultBus Bus is low before the slot or after recovery.
 * @note Poll the same operation until completion before starting another.
 * Pending polls do not restart the slot; their input is ignored.
 * All transfers require running interrupts, never drive PD2 high and
 * preserve the incoming interrupt state. Only short 3..11 us critical
 * intervals retain software delays; Timer0 handles the remaining waits.
 */
EResult_t onewire_write_bit(bool value);
/*----------------------------------------------------------------------------*/

/**
 * @brief Start or poll one standard-speed read slot.
 * @param[out] value High sample as true, low sample as false.
 * @retval eResultBusy Read slot was started or is pending.
 * @retval eResultOk Completed sample was stored.
 * @retval eResultBus Bus is low before the slot or after recovery.
 * @retval eResultInvalidArgument Output is NULL.
 * @note Output is unchanged until Ok; sampling occurs within 15 us.
 * Follow the transfer polling contract of onewire_write_bit().
 */
EResult_t onewire_read_bit(bool *value);
/*----------------------------------------------------------------------------*/

/**
 * @brief Start or poll eight autonomous least-significant-bit-first writes.
 * @param[in] value Byte latched at the start of the operation.
 * @retval eResultBusy Byte was started or is pending.
 * @retval eResultOk All eight write slots completed.
 * @retval eResultBus Bus is low before the byte or after slot recovery.
 * @note Timer0 advances slots without foreground polling.
 * Follow the transfer polling contract of onewire_write_bit().
 */
EResult_t onewire_write_byte(uint8_t value);
/*----------------------------------------------------------------------------*/

/**
 * @brief Start or poll eight autonomous least-significant-bit-first reads.
 * @param[out] value Completed byte.
 * @retval eResultBusy Byte was started or is pending.
 * @retval eResultOk Completed byte was stored.
 * @retval eResultBus Bus is low before the byte or after slot recovery.
 * @retval eResultInvalidArgument Output is NULL.
 * @note Output is unchanged until Ok; Timer0 advances slots autonomously.
 * Follow the transfer polling contract of onewire_write_bit().
 */
EResult_t onewire_read_byte(uint8_t *value);
/*----------------------------------------------------------------------------*/

/**
 * @brief Start or poll two search reads and automatic branch selection.
 * @param[in] direction Preferred branch when both bits are zero.
 * @param[out] bits ID, complement and actual direction in ONEWIRE_SEARCH bits.
 * @retval eResultBusy Triplet was started or is pending.
 * @retval eResultOk Read pair and transmitted branch were stored.
 * @retval eResultBus Idle/recovery fault or both search reads were high.
 * @retval eResultInvalidArgument Output is NULL.
 * @note Both reads high produce Bus without sending a branch bit.
 * Output is unchanged until Ok; a non-collision follows the sole branch.
 * Follow the transfer polling contract of onewire_write_bit().
 */
EResult_t onewire_triplet(bool direction, uint8_t *bits);

/******************************************************************************/
#endif // !ONEWIRE_H
