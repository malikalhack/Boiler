/**
 * @note ATmega16 - 14.7456 MHz
 * @file    Interruptions.h
 * @version 1.0.0
 * @authors Anton Chernov
 * @date    16.05.2019
 * @date    26.08.2022
 */

/******************************************************************************/
#ifndef INTERRUPTIONS_H_
#define INTERRUPTIONS_H_
/******************************* Included files *******************************/
#include <avr/io.h>
#include <avr/interrupt.h>
/****************************** Prototypes API ********************************/
// ISR (INT0_vect);
// ISR (INT1_vect);
ISR (TIMER2_COMP_vect);
// ISR (TIMER2_OVF_vect);
// ISR (TIMER1_CAPT_vect);
// ISR (TIMER1_COMPA_vect);
// ISR (TIMER1_COMPB_vect);
// ISR (TIMER1_OVF_vect);
// ISR (TIMER0_OVF_vect);
// ISR (SPI_STC_vect);
// ISR (USART_RXC_vect);
// ISR (USART_UDRE_vect);
// ISR (USART_TXC_vect);
// ISR (ADC_vect);
// ISR (EE_RDY_vect);
// ISR (ANA_COMP_vect);
// ISR (TWI_vect);
// ISR (INT2_vect);
ISR (TIMER0_COMP_vect);
// ISR (SPM_RDY_vect);
/******************************************************************************/
#endif /* INTERRUPTIONS_H_ */
