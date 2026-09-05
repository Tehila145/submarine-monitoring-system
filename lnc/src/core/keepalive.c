#include "keepalive.h"
#include "records.h"   /* lnc_pack_snapshot */
#include "protocol.h"
#include "tlv.h"

int keepalive_build(uint8_t* buf, uint32_t cap, uint32_t ts, const measurement_t* m) {
    uint8_t inner[48];
    int off = lnc_pack_snapshot(inner, sizeof(inner), ts, m);
    if (off < 0 || off > 255) return -1;
    return tlv_write(buf, cap, RPT_KEEPALIVE, inner, (uint8_t)off);
}
