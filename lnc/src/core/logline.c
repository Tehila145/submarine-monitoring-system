#include "logline.h"
#include <stdio.h>
#include <string.h>

int logline_format_measurement(char* buf, int cap, const measurement_t* m) {
    int n = snprintf(buf, (size_t)cap, "%u,%d,%u,%u,%u,%d\n",
                     (unsigned)m->ts, (int)m->temperature, (unsigned)m->humidity,
                     (unsigned)m->light, (unsigned)m->battery, (int)m->mode);
    if (n < 0 || n >= cap) return -1;
    return n;
}

bool logline_parse_measurement(const char* line, measurement_t* out) {
    unsigned ts, hum, light, batt; int temp, mode;
    if (sscanf(line, "%u,%d,%u,%u,%u,%d", &ts, &temp, &hum, &light, &batt, &mode) != 6)
        return false;
    memset(out, 0, sizeof(*out));
    out->ts = ts; out->temperature = (int16_t)temp; out->humidity = (uint16_t)hum;
    out->light = (uint16_t)light; out->battery = (uint16_t)batt; out->mode = (lnc_mode_t)mode;
    return true;
}

int logline_format_event(char* buf, int cap, const lnc_event_t* e) {
    int from = 0, to = 0, detected = 0, wd = 0;
    if (e->src == SRC_MONITOR) { from = e->data.transition.from; to = e->data.transition.to; }
    else if (e->src == SRC_OBJECT) { detected = e->data.object.detected ? 1 : 0; }
    else if (e->src == SRC_INIT) { wd = e->data.startup.after_wd_reset ? 1 : 0; }
    int n = snprintf(buf, (size_t)cap, "%u,%d,%d,%d,%d,%d\n",
                     (unsigned)e->ts, (int)e->src, from, to, detected, wd);
    if (n < 0 || n >= cap) return -1;
    return n;
}

bool logline_parse_event(const char* line, lnc_event_t* out) {
    unsigned ts; int src, from, to, detected, wd;
    if (sscanf(line, "%u,%d,%d,%d,%d,%d", &ts, &src, &from, &to, &detected, &wd) != 6)
        return false;
    memset(out, 0, sizeof(*out));
    out->ts = ts; out->src = (evt_src_t)src;
    if (src == SRC_MONITOR) { out->data.transition.from = (lnc_mode_t)from;
                              out->data.transition.to = (lnc_mode_t)to; }
    else if (src == SRC_OBJECT) { out->data.object.detected = (detected != 0); }
    else if (src == SRC_INIT) { out->data.startup.after_wd_reset = (wd != 0); }
    return true;
}
