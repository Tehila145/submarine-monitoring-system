#ifndef OBJECTDET_LOGIC_H
#define OBJECTDET_LOGIC_H
#include "lnc_types.h"
void objectdet_logic_reset(void);
bool objectdet_logic_step(bool present_now, uint32_t ts, lnc_event_t* out_event);
#endif
