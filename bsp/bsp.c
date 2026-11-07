/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    bsp.c
 * @version 0.1.11
 * @authors Anton Chernov
 * @date    2026-10-01
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <avr/eeprom.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>

#include "boiler_config.h"
#include "bsp.h"

/********************************* Definitions ********************************/

/**
 * @def BSP_SEGMENT_MASK
 * @brief Data bus pins connected to the display segments.
 */
#define BSP_SEGMENT_MASK    (0xFFU)
/**
 * @def BSP_DIGIT_MASK
 * @brief Port B pins used to select the display digits.
 */
#define BSP_DIGIT_MASK      (_BV(PB0) | _BV(PB1) | _BV(PB2) | \
                             _BV(PB3) | _BV(PB4))
/**
 * @def BSP_BUTTON_MASK
 * @brief Port C pins connected to the user buttons.
 */
#define BSP_BUTTON_MASK     (_BV(PC3) | _BV(PC4) | _BV(PC5) | \
                             _BV(PC6) | _BV(PC7))
/**
 * @def BSP_BUZZER_MASK
 * @brief Port C pin connected to the buzzer output.
 */
#define BSP_BUZZER_MASK     (_BV(PC2))
/**
 * @def BSP_HEATER_1_MASK
 * @brief Port D pin controlling heater output 1.
 */
#define BSP_HEATER_1_MASK   (_BV(PD5))
/**
 * @def BSP_HEATER_2_MASK
 * @brief Port D pin controlling heater output 2.
 */
#define BSP_HEATER_2_MASK   (_BV(PD4))
/**
 * @def BSP_HEATER_MASK
 * @brief Port D pins controlling both heater outputs.
 */
#define BSP_HEATER_MASK     (BSP_HEATER_1_MASK | BSP_HEATER_2_MASK)
/**
 * @def BSP_SYNC_MASK
 * @brief Port D pin connected to the zero-cross synchronization input.
 */
#define BSP_SYNC_MASK       (_BV(PD3))
/**
 * @def BSP_TIM1_PRESCALER
 * @brief Timer1 clock prescaler used for the system tick.
 */
#define BSP_TIM1_PRESCALER  (8UL)

/**
 * @def BSP_TIMER1_COMPARE_VALUE
 * @brief Timer1 compare value for an approximately 1 ms system tick.
 */
#define BSP_TIMER1_COMPARE_VALUE \
    ((F_CPU / BSP_TIM1_PRESCALER / BOILER_TICK_HZ) - 1UL)

/****************************** Module variables ******************************/

/**
 * @var system_tick
 * @brief System tick counter incremented by the Timer1 compare interrupt.
 */
static volatile AcroTick_t system_tick = 0U;

/***************************** Private prototypes *****************************/
/**
 * @brief Configure the board GPIO directions and output levels.
 */
static void bspConfigPorts(void);

/**
 * @brief Configure Timer1 for the system tick period.
 */
static void bspConfigSysTimer(void);

/**
 * @brief Enable the Timer1 compare interrupt and start the timer.
 */
static void bspConfigInts(void);

/**
 * @brief Check whether an EEPROM range fits within the device capacity.
 * @param[in] address First EEPROM address in the range.
 * @param[in] size Number of bytes in the range.
 * @returns true if the non-empty range is within EEPROM bounds.
 */
static bool bspEepromRangeValid(uint16_t address, uint16_t size);

/****************************** Private functions *****************************/

static void bspConfigPorts(void) {
    PORTA = 0U;
    DDRA = BSP_SEGMENT_MASK;

    PORTB = 0U;
    DDRB = BSP_DIGIT_MASK;

    PORTC = BSP_BUTTON_MASK;
    DDRC = BSP_BUZZER_MASK;

    PORTD = BSP_HEATER_MASK | BSP_SYNC_MASK;
    DDRD = BSP_HEATER_MASK;
}

/*----------------------------------------------------------------------------*/

static void bspConfigSysTimer(void) {
    TCCR1A = 0U;
    TCCR1B = 0U;
    TCNT1 = 0U;
    OCR1A = BSP_TIMER1_COMPARE_VALUE;
}

/*----------------------------------------------------------------------------*/

static void bspConfigInts(void) {
    TIFR |= _BV(OCF1A);
    TIMSK |= _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | _BV(CS11);
}

/*----------------------------------------------------------------------------*/

static uint8_t bspGetHeatMask(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = 0U;
    switch (heater) {
        case eBspHeater1:
            heater_mask = BSP_HEATER_1_MASK;
            break;
        case eBspHeater2:
            heater_mask = BSP_HEATER_2_MASK;
            break;
        default:
            break;
    }

    return heater_mask;
}

/*----------------------------------------------------------------------------*/

static bool bspEepromRangeValid(uint16_t address, uint16_t size) {
    uint32_t end_address;
    uint32_t eeprom_size;

    end_address = (uint32_t)address + size;
    eeprom_size = (uint32_t)E2END + 1UL;
    return (size != 0U) && (end_address <= eeprom_size);
}

/********************** Application Programming Interface *********************/

void bspInit(void) {
    bspConfigPorts();
    bspConfigSysTimer();
    bspConfigInts();
}
/*----------------------------------------------------------------------------*/

volatile AcroTick_t *bspGetTick(void) {
    return &system_tick;
}
/*----------------------------------------------------------------------------*/

void bspStart(void) {
    sei();
}
/*----------------------------------------------------------------------------*/

AcroTick_t bsp_get_time(void) {
    AcroTick_t now;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        now = system_tick;
    }
    return now;
}
/*----------------------------------------------------------------------------*/

void bsp_buzzer_set(bool enabled) {
    if (enabled) {
        PORTC |= BSP_BUZZER_MASK;
    }
    else {
        PORTC &= (uint8_t)~BSP_BUZZER_MASK;
    }
}
/*----------------------------------------------------------------------------*/

void bsp_display_write(uint8_t segments, uint8_t select_mask) {
    PORTB &= (uint8_t)~BSP_DIGIT_MASK;
    PORTA = segments;
    PORTB |= select_mask & BSP_DIGIT_MASK;
}
/*----------------------------------------------------------------------------*/

uint8_t bsp_get_buttons(void) {
    return ((uint8_t)~PINC & BSP_BUTTON_MASK) >> PC3;
}
/*----------------------------------------------------------------------------*/

void bspHeaterOn(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = bspGetHeatMask(heater);
    if (heater_mask != 0U) {
        PORTD &= (uint8_t)~heater_mask;
        DDRD |= heater_mask;
    }
}
/*----------------------------------------------------------------------------*/

void bspHeaterOff(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = bspGetHeatMask(heater);
    if (heater_mask != 0U) {
        PORTD |= heater_mask;
        DDRD |= heater_mask;
    }
}
/*----------------------------------------------------------------------------*/

EResult_t bspEepromRead(uint16_t address, uint8_t *data, uint16_t size) {
    EResult_t ret_val;

    ret_val = eResultInvalidArgument;
    if (data != NULL && size != 0U) {
        ret_val = eResultOutOfRange;
        if (bspEepromRangeValid(address, size)) {
            eeprom_read_block(data, (const void *)(uintptr_t)address, size);
            ret_val = eResultOk;
        }
    }

    return ret_val;
}
/*----------------------------------------------------------------------------*/

EResult_t bspEepromUpdate(uint16_t address, const uint8_t *data, uint16_t size) {
    EResult_t ret_val;

    ret_val = eResultInvalidArgument;
    if (data != NULL && size != 0U) {
        ret_val = eResultOutOfRange;
        if (bspEepromRangeValid(address, size)) {
            eeprom_update_block(data, (void *)(uintptr_t)address, size);
            ret_val = eResultOk;
        }
    }

    return ret_val;
}
/*----------------------------------------------------------------------------*/

uint16_t bspEepromSize(void) {
    return (uint16_t)(E2END + 1U);
}
/*----------------------------------------------------------------------------*/

bool bsp_eeprom_ready(void) {
    return eeprom_is_ready();
}
/*----------------------------------------------------------------------------*/

ISR(TIMER1_COMPA_vect) {
    system_tick++;
}
/******************************************************************************/
