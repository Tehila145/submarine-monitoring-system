#include "sched.h"
bool sched_due(uint32_t* last, uint32_t period, uint32_t now) {
    if ((uint32_t)(now - *last) >= period) { *last = now; return true; }
    return false;
}
