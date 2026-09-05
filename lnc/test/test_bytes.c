#include "test_util.h"
#include "bytes.h"

int main(void) {
    uint8_t b[4];
    bytes_put_u16(b, 0x1234);
    EXPECT(b[0] == 0x34 && b[1] == 0x12);
    EXPECT(bytes_get_u16(b) == 0x1234);
    bytes_put_i16(b, -2);
    EXPECT(bytes_get_i16(b) == -2);
    bytes_put_u32(b, 0x11223344u);
    EXPECT(b[0]==0x44 && b[3]==0x11);
    EXPECT(bytes_get_u32(b) == 0x11223344u);
    REPORT();
}
