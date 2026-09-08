#include "records.h"
#include "protocol.h"
#include "tlv.h"
#include "bytes.h"

int lnc_pack_snapshot(uint8_t* inner, uint32_t cap, uint32_t ts, const measurement_t* m) {
    int off = 0, n;
    uint8_t tsb[4]; bytes_put_u32(tsb, ts);
    n = tlv_write(inner+off, cap-(uint32_t)off, TAG_TIMESTAMP, tsb, 4); if (n<0) return -1; off+=n;
    uint8_t meas[8];
    bytes_put_i16(meas+0, m->temperature); bytes_put_u16(meas+2, m->humidity);
    bytes_put_u16(meas+4, m->light);       bytes_put_u16(meas+6, m->battery);
    n = tlv_write(inner+off, cap-(uint32_t)off, TAG_MEASUREMENT, meas, 8); if (n<0) return -1; off+=n;
    uint8_t mode = (uint8_t)m->mode;
    n = tlv_write(inner+off, cap-(uint32_t)off, TAG_MODE, &mode, 1); if (n<0) return -1; off+=n;
    return off;
}

int record_measurement(uint8_t* buf, uint32_t cap, const measurement_t* m) {
    uint8_t inner[48];
    int off = lnc_pack_snapshot(inner, sizeof(inner), m->ts, m);
    if (off < 0 || off > 255) return -1;
    return tlv_write(buf, cap, RPT_DATA, inner, (uint8_t)off);
}

int record_event(uint8_t* buf, uint32_t cap, const lnc_event_t* e) {
    uint8_t inner[48]; int off = 0, n;
    uint8_t tsb[4]; bytes_put_u32(tsb, e->ts);
    n = tlv_write(inner+off, sizeof(inner)-(uint32_t)off, TAG_TIMESTAMP, tsb, 4); if (n<0) return -1; off+=n;
    uint8_t src = (uint8_t)e->src;
    n = tlv_write(inner+off, sizeof(inner)-(uint32_t)off, TAG_EVENT_SRC, &src, 1); if (n<0) return -1; off+=n;
    if (e->src == SRC_MONITOR) {
        uint8_t mm[2] = { (uint8_t)e->data.transition.from, (uint8_t)e->data.transition.to };
        n = tlv_write(inner+off, sizeof(inner)-(uint32_t)off, TAG_MODE, mm, 2); if (n<0) return -1; off+=n;
    }
    if (e->src == SRC_OBJECT) {
        uint8_t flag = e->data.object.detected ? 1u : 0u;   /* so Central can tell DETECTED vs cleared */
        n = tlv_write(inner+off, sizeof(inner)-(uint32_t)off, TAG_EVENT_FLAG, &flag, 1); if (n<0) return -1; off+=n;
    }
    if (off > 255) return -1;
    return tlv_write(buf, cap, RPT_EVENT, inner, (uint8_t)off);
}
