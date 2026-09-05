#include "test_util.h"
#include "lnc_types.h"

int main(void) {
    EXPECT(MODE_ERROR > MODE_WARNING);
    EXPECT(MODE_WARNING > MODE_NORMAL);
    measurement_t m = { .ts = 100, .temperature = -3, .humidity = 40,
                        .light = 500, .battery = 3300, .mode = MODE_NORMAL };
    EXPECT(m.ts == 100u);
    EXPECT(m.temperature == -3);
    EXPECT(m.battery == 3300);
    REPORT();
}
