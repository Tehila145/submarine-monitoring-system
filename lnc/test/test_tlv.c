#include "test_util.h"
#include "tlv.h"

int main(void) {
    uint8_t buf[16];
    uint8_t payload[3] = {0xAA, 0xBB, 0xCC};
    int n = tlv_write(buf, sizeof(buf), 0x21, payload, 3);
    EXPECT(n == 5);
    EXPECT(buf[0] == 0x21 && buf[1] == 0x03);

    uint8_t tag, vlen; const uint8_t* val;
    int c = tlv_read(buf, (uint32_t)n, &tag, &vlen, &val);
    EXPECT(c == 5);
    EXPECT(tag == 0x21 && vlen == 3);
    EXPECT(val[0]==0xAA && val[2]==0xCC);

    uint8_t small[4], big[5] = {0};
    EXPECT(tlv_write(small, sizeof(small), 0x01, big, 5) == -1);   /* overflow */

    uint8_t trunc[2] = {0x21, 0x05};
    EXPECT(tlv_read(trunc, 2, &tag, &vlen, &val) == -1);           /* truncated */
    REPORT();
}
