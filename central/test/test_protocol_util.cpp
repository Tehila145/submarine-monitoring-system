#include "test_util.h"
#include "central/protocol_util.h"
#include "central/codec.h"
#include <vector>

using namespace central;

// Build a keep-alive frame with the exact layout the firmware emits.
static std::vector<uint8_t> makeKeepAlive(uint32_t ts, int16_t t, uint16_t h,
                                          uint16_t l, uint16_t b, uint8_t mode) {
    uint8_t inner[48]; int off = 0, n;
    uint8_t tsb[4]; bytes_put_u32(tsb, ts);
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_TIMESTAMP, tsb, 4); off += n;
    uint8_t meas[8]; bytes_put_i16(meas, t); bytes_put_u16(meas + 2, h);
    bytes_put_u16(meas + 4, l); bytes_put_u16(meas + 6, b);
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_MEASUREMENT, meas, 8); off += n;
    uint8_t md = mode;
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_MODE, &md, 1); off += n;
    std::vector<uint8_t> f(off + 2);
    tlv_write(f.data(), (uint32_t)f.size(), RPT_KEEPALIVE, inner, (uint8_t)off);
    return f;
}

int main() {
    // command encoding
    auto c = cmdSetRtc(1757000000u);
    EXPECT(c.size() == 6 && c[0] == CMD_SET_RTC && c[1] == 4);
    EXPECT(bytes_get_u32(c.data() + 2) == 1757000000u);

    auto g = cmdGetDataRange(100, 200);
    EXPECT(g[0] == CMD_GET_DATA_RANGE && g[1] == 8);
    EXPECT(bytes_get_u32(g.data() + 2) == 100 && bytes_get_u32(g.data() + 6) == 200);

    auto tn = cmdSetTempNormal(-5, 30);
    EXPECT(tn[0] == CMD_SET_TEMP_NORMAL && tn[1] == 4);
    EXPECT(bytes_get_i16(tn.data() + 2) == -5 && bytes_get_i16(tn.data() + 4) == 30);

    auto hb = cmdSetLowerBound(CMD_SET_HUM_NORMAL, 40);
    EXPECT(hb[0] == CMD_SET_HUM_NORMAL && hb[1] == 2 && bytes_get_u16(hb.data() + 2) == 40);

    // parse a keep-alive
    auto ka = makeKeepAlive(946684817u, 25, 54, 700, 3300, 1);
    Report r = parseReport(ka.data(), ka.size());
    EXPECT(r.type == ReportType::KeepAlive);
    EXPECT(r.m.ts == 946684817u && r.m.temp == 25 && r.m.hum == 54 &&
           r.m.light == 700 && r.m.batt == 3300 && r.m.mode == 1);

    // parse an event: MONITOR NORMAL->ERROR
    uint8_t inner[32]; int off = 0, n;
    uint8_t tsb[4]; bytes_put_u32(tsb, 946684955u);
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_TIMESTAMP, tsb, 4); off += n;
    uint8_t src = 0;
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_EVENT_SRC, &src, 1); off += n;
    uint8_t mm[2] = {0, 2};
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_MODE, mm, 2); off += n;
    std::vector<uint8_t> ev(off + 2);
    tlv_write(ev.data(), (uint32_t)ev.size(), RPT_EVENT, inner, (uint8_t)off);
    Report re = parseReport(ev.data(), ev.size());
    EXPECT(re.type == ReportType::Event && re.e.ts == 946684955u &&
           re.e.src == 0 && re.e.from_mode == 0 && re.e.to_mode == 2);

    // parse RSP_TIME
    uint8_t tb[4]; bytes_put_u32(tb, 12345u);
    std::vector<uint8_t> tf(6);
    tlv_write(tf.data(), (uint32_t)tf.size(), RSP_TIME, tb, 4);
    Report rt = parseReport(tf.data(), tf.size());
    EXPECT(rt.type == ReportType::Time && rt.time == 12345u);
    REPORT();
}
