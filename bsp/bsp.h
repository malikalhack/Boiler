/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    bsp.h
 * @version 0.1.8
 * @authors Anton Chernov
 * @date    2026-10-01
 * @date    @showdate "%Y-%m-%d"
 */

#ifndef BSP_H
#define BSP_H
/******************************** Included files ******************************/
#include <stdbool.h>
#include <stdint.h>

#include "acrosched.h"
#include "errors.h"
/********************************* Definitions ********************************/

/**
 * @enum EBspHeater
 * @brief Selects one of the two heater outputs.
 */
typedef enum EBspHeater {
    eBspHeater1 = 0, /**< First heater output on PD4. */
    eBspHeater2 = 1  /**< Second heater output on PD5. */
} EBspHeater_t;

/********************** Application Programming Interface *********************/

/**
 * @brief Configure the board ports, system timer, and timer interrupt.
 */
void bspInit(void);

/**
 * @brief Return a pointer to the system tick counter.
 * @returns Pointer to the volatile system tick counter.
 */
volatile AcroTick_t *bspGetTick(void);

/**
 * @brief Read an atomic snapshot of the system tick counter.
 * @returns Current tick count with the caller's interrupt state preserved.
 */
AcroTick_t bsp_get_time(void);

/**
 * @brief Switch the active-high, self-oscillating buzzer on PC2.
 * @param[in] enabled True to sound; false to silence.
 */
void bsp_buzzer_set(bool enabled);

/**
 * @brief Read active-low PC3..PC7 buttons without changing port outputs.
 * @returns Pressed bits: bit 0 Down, 1 Right, 2 Up, 3 Left, 4 Enter.
 */
uint8_t bsp_get_buttons(void);

/**
 * @brief Enable global interrupts.
 */
void bspStart(void);

/**
 * @brief Disable all selections, update the bus, then enable selections.
 * @param[in] segments Active-high PA0..PA7 segment or LED data.
 * @param[in] select_mask Active-high PB0..PB4 selections; upper bits ignored.
 * Pass zero to leave all positions disabled. Scanning uses one-hot masks.
 * @note Call only from startup or cooperative tasks, not from interrupts.
 */
void bsp_display_write(uint8_t segments, uint8_t select_mask);

/**
 * @brief Enable the selected heater output.
 * @param[in] heater Heater output to enable.
 */
void bspHeaterOn(EBspHeater_t heater);

/**
 * @brief Disable the selected heater output.
 * @param[in] heater Heater output to disable.
 */
void bspHeaterOff(EBspHeater_t heater);

/**
 * @brief Read a range of bytes from EEPROM.
 * @param[in] address First EEPROM address to read.
 * @param[out] data Destination buffer.
 * @param[in] size Number of bytes to read.
 * @retval eResultOk The AVR EEPROM read completed.
 * @retval eResultInvalidArgument Buffer is NULL or size is zero.
 * @retval eResultOutOfRange The range exceeds EEPROM capacity.
 * @note AVR EEPROM reads provide no hardware failure status.
 */
EResult_t bspEepromRead(uint16_t address, uint8_t *data, uint16_t size);

/**
 * @brief Update EEPROM bytes without rewriting unchanged values.
 * @param[in] address First EEPROM address to update.
 * @param[in] data Source buffer.
 * @param[in] size Number of bytes to update.
 * @retval eResultOk The AVR EEPROM update completed.
 * @retval eResultInvalidArgument Buffer is NULL or size is zero.
 * @retval eResultOutOfRange The range exceeds EEPROM capacity.
 * @note AVR updates provide no hardware failure status; storage verifies
 * the record by reading it back.
 */
EResult_t bspEepromUpdate(uint16_t address, const uint8_t *data, uint16_t size);

/**
 * @brief Return the EEPROM capacity in bytes.
 * @returns EEPROM capacity in bytes.
 */
uint16_t bspEepromSize(void);

/**
 * @brief Check whether an EEPROM access can start without waiting.
 * @returns True when the EEPROM write engine is idle.
 */
bool bsp_eeprom_ready(void);
/******************************************************************************/
#endif // !BSP_H
