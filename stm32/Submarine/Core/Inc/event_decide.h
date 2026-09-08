#ifndef EVENT_DECIDE_H
#define EVENT_DECIDE_H
#include "lnc_types.h"
typedef struct {
    bool set_led; led_color_t led;
    bool set_alarm; bool alarm_on;
    bool resume_ops; bool suppress_ops;
} event_action_t;
event_action_t event_decide(const lnc_event_t* e);
#endif
