#include "central/protocol_util.h"
#include "central/codec.h"

namespace central {

// Build a TLV frame [tag][len][value...] into a vector.
static std::vector<uint8_t> frame(uint8_t tag, const uint8_t* val, uint8_t len) {
    std::vector<uint8_t> f(len + 2);
    int n = tlv_write(f.data(), (uint32_t)f.size(), tag, val, len);
    if (n < 0) f.clear();
    return f;
}

std::vector<uint8_t> cmdSetTempNormal(int16_t lo, int16_t hi) {
    uint8_t v[4]; bytes_put_i16(v, lo); bytes_put_i16(v + 2, hi);
    return frame(CMD_SET_TEMP_NORMAL, v, 4);
}
std::vector<uint8_t> cmdSetTempWarning(int16_t lo, int16_t hi) {
    uint8_t v[4]; bytes_put_i16(v, lo); bytes_put_i16(v + 2, hi);
    return frame(CMD_SET_TEMP_WARNING, v, 4);
}
std::vector<uint8_t> cmdSetLowerBound(uint8_t cmdTag, uint16_t val) {
    uint8_t v[2]; bytes_put_u16(v, val);
    return frame(cmdTag, v, 2);
}
std::vector<uint8_t> cmdSetRtc(uint32_t epoch) {
    uint8_t v[4]; bytes_put_u32(v, epoch);
    return frame(CMD_SET_RTC, v, 4);
}
std::vector<uint8_t> cmdGetTime() {
    return frame(CMD_GET_TIME, nullptr, 0);
}
std::vector<uint8_t> cmdGetDataRange(uint32_t from, uint32_t to) {
    uint8_t v[8]; bytes_put_u32(v, from); bytes_put_u32(v + 4, to);
    return frame(CMD_GET_DATA_RANGE, v, 8);
}
std::vector<uint8_t> cmdGetEventsRange(uint32_t from, uint32_t to) {
    uint8_t v[8]; bytes_put_u32(v, from); bytes_put_u32(v + 4, to);
    return frame(CMD_GET_EVENTS_RANGE, v, 8);
}

// Find a child TLV with the given tag inside a value buffer; returns its value
// pointer via *out and length via *olen, or false if absent.
static bool child(const uint8_t* val, uint8_t vlen, uint8_t want,
                  const uint8_t** out, uint8_t* olen) {
    uint32_t i = 0;
    while (i + 2 <= vlen) {
        uint8_t t, l; const uint8_t* v;
        int c = tlv_read(val + i, vlen - i, &t, &l, &v);
        if (c < 0) return false;
        if (t == want) { *out = v; *olen = l; return true; }
        i += (uint32_t)c;
    }
    return false;
}

Report parseReport(const uint8_t* fr, std::size_t len) {
    Report r;
    uint8_t tag, vlen; const uint8_t* val;
    if (tlv_read(fr, (uint32_t)len, &tag, &vlen, &val) < 0) return r;

    const uint8_t* cv; uint8_t cl;
    if (tag == RPT_KEEPALIVE || tag == RPT_DATA) {
        r.type = (tag == RPT_KEEPALIVE) ? ReportType::KeepAlive : ReportType::Data;
        if (child(val, vlen, TAG_TIMESTAMP, &cv, &cl) && cl == 4) r.m.ts = bytes_get_u32(cv);
        if (child(val, vlen, TAG_MEASUREMENT, &cv, &cl) && cl == 8) {
            r.m.temp  = bytes_get_i16(cv);
            r.m.hum   = bytes_get_u16(cv + 2);
            r.m.light = bytes_get_u16(cv + 4);
            r.m.batt  = bytes_get_u16(cv + 6);
        }
        if (child(val, vlen, TAG_MODE, &cv, &cl) && cl >= 1) r.m.mode = cv[0];
    } else if (tag == RPT_EVENT) {
        r.type = ReportType::Event;
        if (child(val, vlen, TAG_TIMESTAMP, &cv, &cl) && cl == 4) r.e.ts = bytes_get_u32(cv);
        if (child(val, vlen, TAG_EVENT_SRC, &cv, &cl) && cl >= 1) r.e.src = cv[0];
        if (child(val, vlen, TAG_MODE, &cv, &cl) && cl == 2) { r.e.from_mode = cv[0]; r.e.to_mode = cv[1]; }
    } else if (tag == RSP_TIME) {
        r.type = ReportType::Time;
        if (vlen >= 4) r.time = bytes_get_u32(val);
    }
    return r;
}

}
