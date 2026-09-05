#ifndef MONITOR_LOGIC_H
#define MONITOR_LOGIC_H
#include "lnc_types.h"
void monitor_logic_reset(void);
bool monitor_logic_step(const measurement_t* m_evaluated, lnc_event_t* out_event);
#endif
