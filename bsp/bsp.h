/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    bsp.h
 * @version 0.2.0
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
/********************************* Definitions ********************************/

/**
 * @enum EBspHeater
 * @brief Selects one of the two heater outputs.
 */
typedef enum EBspHeater {
    eBspHeater1 = 0,
    eBspHeater2 = 1
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
 * @brief Enable global interrupts.
 */
void bspStart(void);

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
 * @returns true if the read succeeds; otherwise false.
 */
bool bspEepromRead(uint16_t address, uint8_t *data, uint16_t size);

/**
 * @brief Update EEPROM bytes without rewriting unchanged values.
 * @param[in] address First EEPROM address to update.
 * @param[in] data Source buffer.
 * @param[in] size Number of bytes to update.
 * @returns true if the update succeeds; otherwise false.
 */
bool bspEepromUpdate(uint16_t address, const uint8_t *data, uint16_t size);

/**
 * @brief Return the EEPROM capacity in bytes.
 * @returns EEPROM capacity in bytes.
 */
uint16_t bspEepromSize(void);
/******************************************************************************/
#endif // !BSP_H
