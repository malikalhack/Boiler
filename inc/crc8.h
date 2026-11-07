/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    crc8.h
 * @version 0.1.7
 * @authors Anton Chernov
 * @date    2026-11-07
 * @date    @showdate "%Y-%m-%d"
 */
#ifndef CRC8_H
#define CRC8_H

/******************************** Included files ******************************/
#include <stdint.h>
/********************************* Definitions ********************************/

/**
 * @def CRC8_POLYNOMIAL
 * @brief Reflected Dallas/Maxim CRC-8 polynomial.
 */
#define CRC8_POLYNOMIAL (0x8CU)

/**
 * @def CRC8_BYTE_BITS
 * @brief Number of bits processed in each CRC input byte.
 */
#define CRC8_BYTE_BITS (8U)

/********************** Application Programming Interface *********************/

/**
 * @brief Calculate the Dallas/Maxim CRC-8 of a byte sequence.
 * @param[in] bytes Input bytes; must be valid when size is nonzero.
 * @param[in] size Number of input bytes.
 * @returns CRC-8 remainder, or zero for an empty input.
 * @note A complete valid ROM or scratchpad, including its CRC, yields zero.
 */
static inline uint8_t crc8(const uint8_t *bytes, uint8_t size) {
    uint8_t crc = 0U;
    uint8_t bit;

    while (size-- != 0U) {
        crc ^= *bytes++;
        for (bit = 0U; bit < CRC8_BYTE_BITS; bit++) {
            crc = (crc >> 1U) ^
                  ((crc & 1U) != 0U ? CRC8_POLYNOMIAL : 0U);
        }
    }
    return crc;
}
/******************************************************************************/
#endif // !CRC8_H
