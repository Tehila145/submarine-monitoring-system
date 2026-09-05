#include "event_decide.h"

event_action_t event_decide(const lnc_event_t* e) {
    event_action_t a = {0};
    if (e->src == SRC_MONITOR) {
        lnc_mode_t from = e->data.transition.from;
        lnc_mode_t to   = e->data.transition.to;
        if (to == MODE_ERROR) {
            a.set_led = true; a.led = LED_RED;
            a.set_alarm = true; a.alarm_on = true; a.suppress_ops = true;
        } else if (to == MODE_WARNING) {
            a.set_led = true; a.led = LED_YELLOW;
            if (from == MODE_ERROR) { a.set_alarm = true; a.alarm_on = false; a.resume_ops = true; }
        } else {
            a.set_led = true; a.led = LED_GREEN;
            if (from == MODE_ERROR) { a.set_alarm = true; a.alarm_on = false; a.resume_ops = true; }
        }
    } else if (e->src == SRC_OBJECT) {
        if (e->data.object.detected) {
            a.set_led = true; a.led = LED_RED; a.set_alarm = true; a.alarm_on = true;
        } else {
            a.set_led = true; a.led = LED_GREEN; a.set_alarm = true; a.alarm_on = false;
        }
    }
    return a;   /* SRC_CONFIG / SRC_INIT: record-only */
}
