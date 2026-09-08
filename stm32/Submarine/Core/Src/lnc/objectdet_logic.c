#include "objectdet_logic.h"

static bool s_present;

void objectdet_logic_reset(void) { s_present = false; }

bool objectdet_logic_step(bool present_now, uint32_t ts, lnc_event_t* out) {
    if (present_now == s_present) return false;
    s_present = present_now;
    out->src = SRC_OBJECT;
    out->ts = ts;
    out->data.object.detected = present_now;
    return true;
}
