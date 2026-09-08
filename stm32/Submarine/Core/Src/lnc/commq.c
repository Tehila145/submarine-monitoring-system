#include "commq.h"
int commq_next(const commq_state_t* s) {
    if (s->has_keepalive) return PRIO_KEEPALIVE;
    if (s->has_event)     return PRIO_EVENT;
    if (s->has_data)      return PRIO_DATA;
    return -1;
}
