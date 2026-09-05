#include "test_util.h"
#include "records.h"
#include "protocol.h"
#include "tlv.h"

int main(void) {
    measurement_t m = { .ts=1000, .temperature=10, .humidity=50, .light=600,
                        .battery=3200, .mode=MODE_NORMAL };
    uint8_t buf[64];
    int n = record_measurement(buf, sizeof(buf), &m);
    EXPECT(n > 0 && buf[0] == RPT_DATA);

    lnc_event_t e = {0}; e.src = SRC_MONITOR; e.ts = 2000;
    e.data.transition.from = MODE_NORMAL; e.data.transition.to = MODE_ERROR;
    n = record_event(buf, sizeof(buf), &e);
    EXPECT(n > 0 && buf[0] == RPT_EVENT);
    uint8_t tag, vlen; const uint8_t* val;
    EXPECT(tlv_read(buf, (uint32_t)n, &tag, &vlen, &val) == n);
    REPORT();
}
