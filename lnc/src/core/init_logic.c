#include "init_logic.h"
#include <string.h>
void init_logic_startup_event(reset_cause_t cause, uint32_t ts, lnc_event_t* out) {
    memset(out, 0, sizeof(*out));
    out->src = SRC_INIT;
    out->ts = ts;
    out->data.startup.after_wd_reset = (cause == RESET_WATCHDOG);
}
