#include "monitor_logic.h"

static bool s_have_prev;
static lnc_mode_t s_prev;

void monitor_logic_reset(void) { s_have_prev = false; s_prev = MODE_NORMAL; }

bool monitor_logic_step(const measurement_t* m, lnc_event_t* out) {
    bool changed = s_have_prev && (m->mode != s_prev);
    if (changed) {
        out->src = SRC_MONITOR;
        out->ts = m->ts;
        out->data.transition.from = s_prev;
        out->data.transition.to = m->mode;
        out->data.transition.m = *m;
    }
    s_prev = m->mode;
    s_have_prev = true;
    return changed;
}
