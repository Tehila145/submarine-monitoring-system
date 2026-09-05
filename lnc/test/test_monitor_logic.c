#include "test_util.h"
#include "monitor_logic.h"

static measurement_t mk(lnc_mode_t mode, uint32_t ts) {
    measurement_t m = {0}; m.mode = mode; m.ts = ts; return m;
}

int main(void) {
    monitor_logic_reset();
    lnc_event_t e;
    measurement_t a = mk(MODE_NORMAL, 1000);
    EXPECT(monitor_logic_step(&a, &e) == false);   /* first sample: no change */

    measurement_t b = mk(MODE_ERROR, 1005);
    EXPECT(monitor_logic_step(&b, &e) == true);     /* normal -> error */
    EXPECT(e.src == SRC_MONITOR);
    EXPECT(e.data.transition.from == MODE_NORMAL);
    EXPECT(e.data.transition.to == MODE_ERROR);
    EXPECT(e.ts == 1005u);

    measurement_t c = mk(MODE_ERROR, 1010);
    EXPECT(monitor_logic_step(&c, &e) == false);    /* still error: no event */
    REPORT();
}
