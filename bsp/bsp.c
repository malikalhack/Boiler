/**
 * @copyright @showdate "%Y " Anton Chernov. All rights reserved.
 * @file    bsp.c
 * @version 0.2.0
 * @authors Anton Chernov
 * @date    2026-10-01
 * @date    @showdate "%Y-%m-%d"
 */

/******************************** Included files ******************************/
#include <avr/io.h>
#include <avr/interrupt.h>

#include "boiler_config.h"
#include "bsp.h"

/********************************* Definitions ********************************/
#define BSP_SEGMENT_MASK    (0xFFU)
#define BSP_DIGIT_MASK      (_BV(PB0) | _BV(PB1) | _BV(PB2) | \
                             _BV(PB3) | _BV(PB4))
#define BSP_BUTTON_MASK     (_BV(PC3) | _BV(PC4) | _BV(PC5) | \
                             _BV(PC6) | _BV(PC7))
#define BSP_BUZZER_MASK     (_BV(PC2))
#define BSP_HEATER_1_MASK   (_BV(PD5))
#define BSP_HEATER_2_MASK   (_BV(PD4))
#define BSP_HEATER_MASK     (BSP_HEATER_1_MASK | BSP_HEATER_2_MASK)
#define BSP_SYNC_MASK       (_BV(PD3))
#define BSP_TIMER1_PRESCALER (8UL)

/**
 * @def BSP_TIMER1_COMPARE_VALUE
 * @brief Timer1 compare value for an approximately 1 ms system tick.
 */
#define BSP_TIMER1_COMPARE_VALUE \
    ((F_CPU / BSP_TIMER1_PRESCALER / BOILER_TICK_HZ) - 1UL)

/****************************** Module variables ******************************/

static volatile AcroTick_t system_tick = 0U;

/***************************** Private prototypes *****************************/
static void bspConfigPorts(void);
static void bspConfigSysTimer(void);
static void bspConfigInts(void);
/***************************** Private functions *****************************/

/** @fn bspConfigPorts */
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

/** @fn bspConfigSysTimer */
static void bspConfigSysTimer(void) {
    TCCR1A = 0U;
    TCCR1B = 0U;
    TCNT1 = 0U;
    OCR1A = BSP_TIMER1_COMPARE_VALUE;
}

/*----------------------------------------------------------------------------*/

/** @fn bspConfigInts */
static void bspConfigInts(void) {
    TIFR |= _BV(OCF1A);
    TIMSK |= _BV(OCIE1A);
    TCCR1B = _BV(WGM12) | _BV(CS11);
}

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

/********************* Application Programming Interface *********************/

/** @fn bspInit */
void bspInit(void) {
    bspConfigPorts();
    bspConfigSysTimer();
    bspConfigInts();
}

/*----------------------------------------------------------------------------*/

/** @fn bspGetTick */
volatile AcroTick_t *bspGetTick(void) {
    return &system_tick;
}

/*----------------------------------------------------------------------------*/

/** @fn bspStart */
void bspStart(void) {
    sei();
}

/*----------------------------------------------------------------------------*/

/** @fn bspHeaterOn */
void bspHeaterOn(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = bspGetHeatMask(heater);
    if (heater_mask != 0U) {
        PORTD &= (uint8_t)~heater_mask;
        DDRD |= heater_mask;
    }
}

/*----------------------------------------------------------------------------*/

/** @fn bspHeaterOff */
void bspHeaterOff(EBspHeater_t heater) {
    uint8_t heater_mask;

    heater_mask = bspGetHeatMask(heater);
    if (heater_mask != 0U) {
        PORTD |= heater_mask;
        DDRD |= heater_mask;
    }
}

/*----------------------------------------------------------------------------*/

/** @fn TIMER1_COMPA_vect */
ISR(TIMER1_COMPA_vect) {
    system_tick++;
}
/******************************************************************************/