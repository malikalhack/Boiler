#include <avr/io.h>
#include "standard.h"
/******************************** Definition **********************************/
#define INVERSE     /* the common wire is connected through the inverter  */
#define CATOD       /* If declared, then indicator with a common cathode,
                       otherwise - with a common anode */

#define LCI_DDR     DDRA
#define LCI_PORT    PORTA
#define LCICOM_DDR  DDRB
#define LCICOM_PORT PORTB
#define COM1        (0)
#define COM2        (1)
#define COM3        (2)
#define COM4        (3)
#define COM5        (4)

#define OR_MASK     (BIT(COM1)|BIT(COM2)|BIT(COM3)|BIT(COM4)|BIT(COM5))
#define AND_MASK    ~OR_MASK

#ifdef CATOD
    #ifndef INVERSE
        #define COM_ON(a)	LCICOM_PORT&=~(BIT(a))
        #define COM_OFF(a)	LCICOM_PORT|=BIT(a)
        #define COM_LCI_ON	LCICOM_PORT&=AND_MASK
        #define COM_LCI_OFF	LCICOM_PORT|=OR_MASK
    #else
        #define COM_ON(a)	LCICOM_PORT|=BIT(a)
        #define COM_OFF(a)	LCICOM_PORT&=~(BIT(a))
        #define COM_LCI_ON	LCICOM_PORT|=OR_MASK
        #define COM_LCI_OFF	LCICOM_PORT&=AND_MASK
    #endif
#else
    #ifndef INVERSE
        #define COM_ON(a)	LCICOM_PORT&=~(BIT(a))
        #define COM_OFF(a)	LCICOM_PORT|=BIT(a)
        #define COM_LCI_ON	LCICOM_PORT|=OR_MASK
        #define COM_LCI_OFF	LCICOM_PORT&=AND_MASK
    #else
        #define COM_ON(a)	LCICOM_PORT|=BIT(a)
        #define COM_OFF(a)	LCICOM_PORT&=~(BIT(a))
        #define COM_LCI_ON	LCICOM_PORT&=AND_MASK
        #define COM_LCI_OFF	LCICOM_PORT|=OR_MASK
    #endif
#endif
/******************************************************************************/
void bsp_gpio_init (void) {
    DDRA  = 0xFF;
    PORTA = 0x00;
    DDRB  = 0x1F;
    PORTB = 0xE0;
    DDRC  = 0x04;
    PORTC = 0xFB;
#ifdef ONEWIRECONVERTER
    DDRD  = 0x32;
    PORTD = 0xC6;
#else
    DDRD  = 0x30;
    PORTD = 0xC3;
#endif
}
/******************************************************************************/
void bsp_systimer_start(void) {
/*  When CPU frequency equal 14745600Hz
 *  1mS:    clock/64    OCR0=231
 *  500uS:  clock/32    OCR0=231
 *  100uS:  clock/8     OCR0=185
 *  10uS:   clock/1     OCR0=148
 *  1uS:    clock/1     OCR0=15
 */
    TCNT0 =  0;
    OCR0  =  230;
    TIMSK|= BIT(OCIE0);
    TCCR0 = BIT(WGM01) + BIT(CS01) + BIT(CS00);
}
/******************************************************************************/
void bsp_com_lci_off(void) {
    COM_LCI_OFF;
}
/******************************************************************************/
void bsp_com_lci_on(BYTE gpio) {
    static BYTE gpio_com[] = {COM1, COM2, COM3, COM4, COM5};
    COM_ON(*(gpio_com + gpio));
}
/******************************************************************************/
void bsp_send2port(BYTE segment) {
#ifndef CATOD
        segment = ~segment;
#endif
}
/******************************************************************************/
