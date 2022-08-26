#include <avr/io.h>
#include "standard.h"

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