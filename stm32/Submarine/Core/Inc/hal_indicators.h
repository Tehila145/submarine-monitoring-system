#ifndef HAL_INDICATORS_H
#define HAL_INDICATORS_H
#include "lnc_types.h"
void led_init(void);               /* configure the RGB pins (PC6/7/8) as outputs */
void led_set(led_color_t c);
void alarm_set(bool on);
bool button_pressed_clear(void);   /* returns+clears the button-press flag */
void indicators_button_isr(void);  /* call from HAL_GPIO_EXTI_Callback */
#endif
