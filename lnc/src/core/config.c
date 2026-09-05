#include "config.h"
#include "protocol.h"
#include "bytes.h"
#include "config_defaults.h"
#include <string.h>

void config_load_defaults(lnc_config_t* c) {
    c->magic = LNC_CONFIG_MAGIC;
    c->temp_norm_lo = DEF_TEMP_NORM_LO;  c->temp_norm_hi = DEF_TEMP_NORM_HI;
    c->temp_warn_lo = DEF_TEMP_WARN_LO;  c->temp_warn_hi = DEF_TEMP_WARN_HI;
    c->hum_norm_lo  = DEF_HUM_NORM_LO;   c->hum_warn_lo  = DEF_HUM_WARN_LO;
    c->light_norm_lo = DEF_LIGHT_NORM_LO; c->light_warn_lo = DEF_LIGHT_WARN_LO;
    c->batt_norm_lo = DEF_BATT_NORM_LO;  c->batt_warn_lo = DEF_BATT_WARN_LO;
}

uint32_t config_serialize(const lnc_config_t* c, uint8_t* buf, uint32_t cap) {
    if (cap < sizeof(lnc_config_t)) return 0;
    memcpy(buf, c, sizeof(lnc_config_t));
    return (uint32_t)sizeof(lnc_config_t);
}

bool config_deserialize(lnc_config_t* c, const uint8_t* buf, uint32_t len) {
    if (len < sizeof(lnc_config_t)) return false;
    lnc_config_t tmp; memcpy(&tmp, buf, sizeof(lnc_config_t));
    if (tmp.magic != LNC_CONFIG_MAGIC) return false;
    *c = tmp; return true;
}

bool config_apply_set(lnc_config_t* c, uint8_t tag, const uint8_t* v, uint8_t vlen) {
    switch (tag) {
        case CMD_SET_TEMP_NORMAL:
            if (vlen != 4) return false;
            c->temp_norm_lo = bytes_get_i16(v); c->temp_norm_hi = bytes_get_i16(v+2); return true;
        case CMD_SET_TEMP_WARNING:
            if (vlen != 4) return false;
            c->temp_warn_lo = bytes_get_i16(v); c->temp_warn_hi = bytes_get_i16(v+2); return true;
        case CMD_SET_HUM_NORMAL:    if (vlen!=2) return false; c->hum_norm_lo   = bytes_get_u16(v); return true;
        case CMD_SET_HUM_WARNING:   if (vlen!=2) return false; c->hum_warn_lo   = bytes_get_u16(v); return true;
        case CMD_SET_LIGHT_NORMAL:  if (vlen!=2) return false; c->light_norm_lo = bytes_get_u16(v); return true;
        case CMD_SET_LIGHT_WARNING: if (vlen!=2) return false; c->light_warn_lo = bytes_get_u16(v); return true;
        case CMD_SET_BATT_NORMAL:   if (vlen!=2) return false; c->batt_norm_lo  = bytes_get_u16(v); return true;
        case CMD_SET_BATT_WARNING:  if (vlen!=2) return false; c->batt_warn_lo  = bytes_get_u16(v); return true;
        default: return false;
    }
}
