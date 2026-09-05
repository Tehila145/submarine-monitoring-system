#include "test_util.h"
#include "event_decide.h"

static lnc_event_t trans(lnc_mode_t f, lnc_mode_t t) {
    lnc_event_t e = {0}; e.src = SRC_MONITOR;
    e.data.transition.from = f; e.data.transition.to = t; return e;
}

int main(void) {
    lnc_event_t e;
    e = trans(MODE_NORMAL, MODE_WARNING);
    event_action_t a = event_decide(&e);
    EXPECT(a.set_led && a.led == LED_YELLOW && !a.set_alarm);

    e = trans(MODE_WARNING, MODE_ERROR); a = event_decide(&e);
    EXPECT(a.led == LED_RED && a.set_alarm && a.alarm_on && a.suppress_ops);

    e = trans(MODE_ERROR, MODE_WARNING); a = event_decide(&e);
    EXPECT(a.led == LED_YELLOW && a.set_alarm && !a.alarm_on && a.resume_ops);

    e = trans(MODE_WARNING, MODE_NORMAL); a = event_decide(&e);
    EXPECT(a.led == LED_GREEN && !a.set_alarm);

    e = trans(MODE_ERROR, MODE_NORMAL); a = event_decide(&e);
    EXPECT(a.led == LED_GREEN && a.set_alarm && !a.alarm_on && a.resume_ops);

    lnc_event_t od = {0}; od.src = SRC_OBJECT; od.data.object.detected = true;
    a = event_decide(&od);
    EXPECT(a.led == LED_RED && a.alarm_on);
    od.data.object.detected = false; a = event_decide(&od);
    EXPECT(a.led == LED_GREEN && a.set_alarm && !a.alarm_on);

    lnc_event_t cfg = {0}; cfg.src = SRC_CONFIG; a = event_decide(&cfg);
    EXPECT(!a.set_led && !a.set_alarm);   /* record-only */
    REPORT();
}
