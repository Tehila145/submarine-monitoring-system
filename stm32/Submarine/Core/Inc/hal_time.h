#ifndef HAL_TIME_H
#define HAL_TIME_H
#include <stdint.h>
#include <stdbool.h>
uint32_t rtc_now(void);        /* epoch seconds from the RTC calendar */
void     rtc_set(uint32_t epoch);
void     rtc_datestr8(char out[9]);   /* "YYYYMMDD" of the current RTC date */
#endif
