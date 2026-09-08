#include "command.h"
#include "protocol.h"
#include "tlv.h"
#include "bytes.h"
#include <string.h>

command_result_t command_handle(const uint8_t* frame, uint32_t len,
                                lnc_config_t* cfg, uint32_t now_epoch) {
    command_result_t r; memset(&r, 0, sizeof(r));
    uint8_t tag, vlen; const uint8_t* val;
    if (tlv_read(frame, len, &tag, &vlen, &val) < 0) { return r; }  /* handled=false */

    switch (tag) {
        case CMD_SET_TEMP_NORMAL: case CMD_SET_TEMP_WARNING:
        case CMD_SET_HUM_NORMAL:  case CMD_SET_HUM_WARNING:
        case CMD_SET_LIGHT_NORMAL:case CMD_SET_LIGHT_WARNING:
        case CMD_SET_BATT_NORMAL: case CMD_SET_BATT_WARNING:
            r.handled = true;
            if (config_apply_set(cfg, tag, val, vlen)) r.config_changed = true;
            else r.nack = true;
            return r;
        case CMD_SET_RTC:
            r.handled = true;
            if (vlen != 4) { r.nack = true; return r; }
            r.set_rtc = true; r.rtc_epoch = bytes_get_u32(val);
            return r;
        case CMD_GET_TIME: {
            r.handled = true;
            uint8_t tb[4]; bytes_put_u32(tb, now_epoch);
            int n = tlv_write(r.reply, sizeof(r.reply), RSP_TIME, tb, 4);
            if (n < 0) { r.nack = true; return r; }
            r.reply_len = (uint8_t)n;
            return r;
        }
        case CMD_GET_DATA_RANGE:
        case CMD_GET_EVENTS_RANGE:
            r.handled = true;
            if (vlen != 8) { r.nack = true; return r; }
            r.query = (tag == CMD_GET_DATA_RANGE) ? QUERY_DATA : QUERY_EVENTS;
            r.range_from = bytes_get_u32(val);
            r.range_to   = bytes_get_u32(val + 4);
            return r;
        default:
            return r;   /* handled=false */
    }
}
