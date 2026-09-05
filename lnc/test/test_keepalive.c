#include "test_util.h"
#include "keepalive.h"
#include "protocol.h"
#include "tlv.h"

int main(void) {
    measurement_t m = { .ts=0x11223344, .temperature=21, .humidity=40,
                        .light=700, .battery=3300, .mode=MODE_WARNING };
    uint8_t buf[64];
    int n = keepalive_build(buf, sizeof(buf), m.ts, &m);
    EXPECT(n > 0 && buf[0] == RPT_KEEPALIVE);

    uint8_t tag, vlen; const uint8_t* val;
    EXPECT(tlv_read(buf, (uint32_t)n, &tag, &vlen, &val) == n);
    /* first child = timestamp */
    uint8_t ct, cl; const uint8_t* cv;
    EXPECT(tlv_read(val, vlen, &ct, &cl, &cv) > 0);
    EXPECT(ct == TAG_TIMESTAMP && cl == 4);
    REPORT();
}
