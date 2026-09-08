#include "central/ground_protocol.h"
#include "central/codec.h"   // tlv.h + bytes.h

namespace central {

static std::vector<uint8_t> frame(uint8_t tag, const uint8_t* val, uint8_t len) {
    std::vector<uint8_t> f(len + 2u);
    int n = tlv_write(f.data(), (uint32_t)f.size(), tag, val, len);
    if (n < 0) f.clear();
    return f;
}

// Measurement wire layout (13 bytes): u32 ts | i16 temp | u16 hum | u16 light | u16 batt | u8 mode
static void putMeas(uint8_t* v, const Measurement& m) {
    bytes_put_u32(v,      m.ts);
    bytes_put_i16(v + 4,  m.temp);
    bytes_put_u16(v + 6,  m.hum);
    bytes_put_u16(v + 8,  m.light);
    bytes_put_u16(v + 10, m.batt);
    v[12] = m.mode;
}
// Event wire layout (8 bytes): u32 ts | u8 src | u8 from_mode | u8 to_mode | u8 detected
static void putEvent(uint8_t* v, const EventRec& e) {
    bytes_put_u32(v, e.ts);
    v[4] = e.src; v[5] = e.from_mode; v[6] = e.to_mode; v[7] = e.detected ? 1 : 0;
}

std::vector<uint8_t> gsReqLogRange(uint32_t from, uint32_t to) {
    uint8_t v[8]; bytes_put_u32(v, from); bytes_put_u32(v + 4, to);
    return frame(GS_GET_LOG_RANGE, v, 8);
}
std::vector<uint8_t> gsReqEventsRange(uint32_t from, uint32_t to) {
    uint8_t v[8]; bytes_put_u32(v, from); bytes_put_u32(v + 4, to);
    return frame(GS_GET_EVENTS_RANGE, v, 8);
}
std::vector<uint8_t> gsLogRecord(const Measurement& m) {
    uint8_t v[13]; putMeas(v, m); return frame(GS_LOG_RECORD, v, 13);
}
std::vector<uint8_t> gsEventRecord(const EventRec& e) {
    uint8_t v[8]; putEvent(v, e); return frame(GS_EVENT_RECORD, v, 8);
}
std::vector<uint8_t> gsDone(uint32_t count) {
    uint8_t v[4]; bytes_put_u32(v, count); return frame(GS_DONE, v, 4);
}

GsRequest gsParseRequest(const uint8_t* fr, std::size_t len) {
    GsRequest r;
    uint8_t tag, vlen; const uint8_t* val;
    if (tlv_read(fr, (uint32_t)len, &tag, &vlen, &val) < 0) return r;
    if ((tag == GS_GET_LOG_RANGE || tag == GS_GET_EVENTS_RANGE) && vlen == 8) {
        r.tag = tag; r.from = bytes_get_u32(val); r.to = bytes_get_u32(val + 4);
    }
    return r;
}

bool gsDecodeLog(const uint8_t* fr, std::size_t len, Measurement& out) {
    uint8_t tag, vlen; const uint8_t* v;
    if (tlv_read(fr, (uint32_t)len, &tag, &vlen, &v) < 0) return false;
    if (tag != GS_LOG_RECORD || vlen != 13) return false;
    out.ts = bytes_get_u32(v); out.temp = bytes_get_i16(v + 4);
    out.hum = bytes_get_u16(v + 6); out.light = bytes_get_u16(v + 8);
    out.batt = bytes_get_u16(v + 10); out.mode = v[12];
    return true;
}
bool gsDecodeEvent(const uint8_t* fr, std::size_t len, EventRec& out) {
    uint8_t tag, vlen; const uint8_t* v;
    if (tlv_read(fr, (uint32_t)len, &tag, &vlen, &v) < 0) return false;
    if (tag != GS_EVENT_RECORD || vlen != 8) return false;
    out.ts = bytes_get_u32(v); out.src = v[4]; out.from_mode = v[5];
    out.to_mode = v[6]; out.detected = (v[7] != 0);
    return true;
}
bool gsDecodeDone(const uint8_t* fr, std::size_t len, uint32_t& count) {
    uint8_t tag, vlen; const uint8_t* v;
    if (tlv_read(fr, (uint32_t)len, &tag, &vlen, &v) < 0) return false;
    if (tag != GS_DONE || vlen != 4) return false;
    count = bytes_get_u32(v);
    return true;
}

}
