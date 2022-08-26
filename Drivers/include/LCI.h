/**
 * @file    LCI.h
 * @version 1.0.0
 * @authors Anton Chernov
 * @date    14.05.2019
 * @date    26.08.2022
 */
/******************************************************************************/
#ifndef LCI_H
#define LCI_H
/******************************* Included files *******************************/
#include "standard.h"
/********************************* Definition *********************************/
#define f_zero      0x3F    ///< Symbol of zero
#define f_one       0x06    ///< Symbol of one
#define f_two       0x5B    ///< Symbol of two
#define f_three     0x4F    ///< Symbol of three
#define f_four      0x66    ///< Symbol of four
#define f_five      0x6D    ///< Symbol of five
#define f_six       0x7D    ///< Symbol of six
#define f_seven     0x07    ///< Symbol of seven
#define f_eight     0x7F    ///< Symbol of eight
#define f_nine      0x6F    ///< Symbol of nine
#define f_t1        0x63    ///< Special symbol type 1
#define f_t2        0x5C    ///< Special symbol type 2
#define f_unscore   0x08    ///< Symbol of underscore
#define f_dash      0x40    ///< Symbol of dash
#define f_a         0x77    ///< Symbol "A"
#define f_b         0x7C    ///< Symbol "b"
#define f_c         0x58    ///< Symbol "c"
#define f_cc        0x39    ///< Symbol "C"
#define f_d         0x5E    ///< Symbol "d"
#define f_e         0x79    ///< Symbol "e"
#define f_f         0x71    ///< Symbol "F"
#define f_g         0x3D    ///< Symbol "G"
#define f_h         0x74    ///< Symbol "h"
#define f_hc        0x76    ///< Symbol "H"
#define f_i         0x04    ///< Symbol "i"
#define f_ic        0x30    ///< Symbol "I"
#define f_j         0x0E    ///< Symbol "J"
#define f_l         0x38    ///< Symbol "L"
#define f_n         0x54    ///< Symbol "n"
#define f_nc        0x37    ///< Symbol "N"
#define f_o         0x5C    ///< Symbol "o"
#define f_oc        0x3F    ///< Symbol "O"
#define f_p         0x73    ///< Symbol "P"
#define f_q         0x67    ///< Symbol "q"
#define f_qc        0xBF    ///< Symbol "Q"
#define f_r         0x50    ///< Symbol "r"
#define f_s         0x6D    ///< Symbol "S"
#define f_t         0x78    ///< Symbol "T"
#define f_u         0x3E    ///< Symbol "u"
#define f_uc        0x1C    ///< Symbol "U"
#define f_y         0x6E    ///< Symbol "Y"
/******************** Task Manager functions prototypes ***********************/
/**
 * @brief Function of displaying the contents of the buffer on the display.
 * @param ptr void pointer (unused) 
 */
void lci_display (void *);
/******************************* Prototypes API *******************************/
/** @brief Display initialization function */
void lci_init (void);
/******************************************************************************/
/**
 * @brief Function of setting values on the display.
 * @param p_array value buffer
 */
void lci_set (BYTE *);
/******************************************************************************/
/**
 * @brief LED array setting function .
 * @param value 
 */
void lci_set_led (BYTE);
/******************************************************************************/
#endif /* LCI_H */
