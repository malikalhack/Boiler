/**
 * @file    WDT.h
 * @version 1.0.0
 * @authors Anton Chernov
 * @date    06.05.2020
 */
#ifndef WDT_H_
#define WDT_H_
/******************************** Definitions *********************************/
#define WD_16K_CYCLES   0 /* 17.1 ms at Vcc = 3.0V | 16.3 ms at Vcc = 5.0V */
#define WD_32K_CYCLES   1 /* 34.3 ms at Vcc = 3.0V | 32.5 ms at Vcc = 5.0V */
#define WD_64K_CYCLES   2 /* 68.5 ms at Vcc = 3.0V | 65.0 ms at Vcc = 5.0V */
#define WD_128K_CYCLES  3 /* 0.14 s  at Vcc = 3.0V | 0.13 s  at Vcc = 5.0V */
#define WD_256K_CYCLES  4 /* 0.27 s  at Vcc = 3.0V | 0.26 s  at Vcc = 5.0V */
#define WD_512K_CYCLES  5 /* 0.55 s  at Vcc = 3.0V | 0.52 s  at Vcc = 5.0V */
#define WD_1024K_CYCLES 6 /* 1.10 s  at Vcc = 3.0V | 1.00 s  at Vcc = 5.0V */
#define WD_2048K_CYCLES 7 /* 2.20 s  at Vcc = 3.0V | 2.10 s  at Vcc = 5.0V */
/****************************** Included files ********************************/
#include "standard.h" /* Standard Header Files */
/******************** Task Manager functions prototypes ***********************/
/**
 * @brief Task manager function to reset the watchdog timer.
 * @param[in] ptr void pointer (unused)
 */
void wd_throw_bone (void *);

/****************************** Prototypes API ********************************/
/**
 * @brief Watchdog timer setting function.
 * @param[in] ptr pointer to parameter structure
 */
void wdt_init(const BYTE);

/** @brief Watchdog start function. */
void wdt_start(void);

/** @brief Watchdog stop function. */
void wdt_stop(void);
/******************************************************************************/
#endif /* WDT_H_ */
