#include "lnc_limits.h"

lnc_mode_t limits_classify_range(int16_t v, int16_t nlo, int16_t nhi, int16_t wlo, int16_t whi) {
    if (v >= nlo && v <= nhi) return MODE_NORMAL;
    if (v >= wlo && v <= whi) return MODE_WARNING;
    return MODE_ERROR;
}
lnc_mode_t limits_classify_lower(uint16_t v, uint16_t nlo, uint16_t wlo) {
    if (v >= nlo) return MODE_NORMAL;
    if (v >= wlo) return MODE_WARNING;
    return MODE_ERROR;
}
static lnc_mode_t worst(lnc_mode_t a, lnc_mode_t b) { return a > b ? a : b; }
lnc_mode_t limits_evaluate(const measurement_t* m, const lnc_config_t* c) {
    lnc_mode_t mode = limits_classify_range(m->temperature,
        c->temp_norm_lo, c->temp_norm_hi, c->temp_warn_lo, c->temp_warn_hi);
    mode = worst(mode, limits_classify_lower(m->humidity, c->hum_norm_lo, c->hum_warn_lo));
    mode = worst(mode, limits_classify_lower(m->light,   c->light_norm_lo, c->light_warn_lo));
    mode = worst(mode, limits_classify_lower(m->battery, c->batt_norm_lo, c->batt_warn_lo));
    return mode;
}
