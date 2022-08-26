#ifndef BSP_H_
#define BSP_H_
/******************************** Definition **********************************/
#define MAX_SIGN_NUM 5  /* Display Width */
/****************************** Prototypes API ********************************/
void bsp_gpio_init (void);
void bsp_systimer_start(void);
void bsp_com_lci_off(void);
void bsp_com_lci_on(BYTE);  //register value only
void bsp_send2port(BYTE);   //register value only
/******************************************************************************/
#endif /* BSP_H_ */
