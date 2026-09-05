#include "test_util.h"
#include "command.h"
#include "init_logic.h"
#include "protocol.h"
#include "tlv.h"
#include "bytes.h"

/* build a command frame [tag][len][val] into f, return length */
static uint32_t frame(uint8_t* f, uint8_t tag, const uint8_t* v, uint8_t n) {
    return (uint32_t)tlv_write(f, 32, tag, v, n);
}

int main(void) {
    lnc_config_t cfg; config_load_defaults(&cfg);
    uint8_t f[32];

    /* a config SET flows through command_handle */
    uint8_t v2[2]; bytes_put_u16(v2, 77);
    uint32_t n = frame(f, CMD_SET_HUM_NORMAL, v2, 2);
    command_result_t r = command_handle(f, n, &cfg, 5000);
    EXPECT(r.handled && r.config_changed && cfg.hum_norm_lo == 77);

    /* SET_RTC reports the epoch for the caller to apply */
    uint8_t v4[4]; bytes_put_u32(v4, 1700000000u);
    n = frame(f, CMD_SET_RTC, v4, 4);
    r = command_handle(f, n, &cfg, 5000);
    EXPECT(r.handled && r.set_rtc && r.rtc_epoch == 1700000000u);

    /* GET_TIME builds an RSP_TIME reply carrying now_epoch */
    n = frame(f, CMD_GET_TIME, NULL, 0);
    r = command_handle(f, n, &cfg, 12345);
    EXPECT(r.handled && r.reply_len > 0 && r.reply[0] == RSP_TIME);
    uint8_t tag, vlen; const uint8_t* val;
    EXPECT(tlv_read(r.reply, r.reply_len, &tag, &vlen, &val) > 0);
    EXPECT(vlen == 4 && bytes_get_u32(val) == 12345u);

    /* GET_DATA_RANGE / GET_EVENTS_RANGE parse the window */
    uint8_t v8[8]; bytes_put_u32(v8, 100); bytes_put_u32(v8+4, 200);
    n = frame(f, CMD_GET_DATA_RANGE, v8, 8);
    r = command_handle(f, n, &cfg, 0);
    EXPECT(r.query == QUERY_DATA && r.range_from == 100 && r.range_to == 200);
    n = frame(f, CMD_GET_EVENTS_RANGE, v8, 8);
    r = command_handle(f, n, &cfg, 0);
    EXPECT(r.query == QUERY_EVENTS);

    /* unknown tag / malformed length */
    n = frame(f, 0x99, v2, 2);
    r = command_handle(f, n, &cfg, 0);
    EXPECT(!r.handled);
    n = frame(f, CMD_SET_HUM_NORMAL, v2, 1);   /* wrong len */
    r = command_handle(f, n, &cfg, 0);
    EXPECT(r.handled && r.nack);

    /* init startup event */
    lnc_event_t e;
    init_logic_startup_event(RESET_WATCHDOG, 42, &e);
    EXPECT(e.src == SRC_INIT && e.ts == 42u && e.data.startup.after_wd_reset);
    init_logic_startup_event(RESET_NORMAL, 43, &e);
    EXPECT(!e.data.startup.after_wd_reset);
    REPORT();
}
