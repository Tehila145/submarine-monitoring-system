#ifndef HAL_WATCHDOG_H
#define HAL_WATCHDOG_H
#include "lnc_types.h"
void          wd_refresh(void);
reset_cause_t reset_cause(void);
#endif
