#include "test_util.h"
#include "lnc_limits.h"

int main(void) {
    /* range: normal 10..30, warning 0..40 */
    EXPECT(limits_classify_range(20, 10, 30, 0, 40) == MODE_NORMAL);
    EXPECT(limits_classify_range(35, 10, 30, 0, 40) == MODE_WARNING);
    EXPECT(limits_classify_range(5,  10, 30, 0, 40) == MODE_WARNING);
    EXPECT(limits_classify_range(45, 10, 30, 0, 40) == MODE_ERROR);
    EXPECT(limits_classify_range(-5, 10, 30, 0, 40) == MODE_ERROR);

    /* lower: normal >=600, warning >=400 */
    EXPECT(limits_classify_lower(700, 600, 400) == MODE_NORMAL);
    EXPECT(limits_classify_lower(500, 600, 400) == MODE_WARNING);
    EXPECT(limits_classify_lower(300, 600, 400) == MODE_ERROR);

    lnc_config_t c = {0};
    c.temp_norm_lo=10; c.temp_norm_hi=30; c.temp_warn_lo=0; c.temp_warn_hi=40;
    c.hum_norm_lo=40; c.hum_warn_lo=20; c.light_norm_lo=600; c.light_warn_lo=400;
    c.batt_norm_lo=3000; c.batt_warn_lo=2500;
    measurement_t m = { .temperature=20, .humidity=45, .light=700, .battery=3300 };
    EXPECT(limits_evaluate(&m, &c) == MODE_NORMAL);
    m.battery = 2700; EXPECT(limits_evaluate(&m, &c) == MODE_WARNING);
    m.temperature = 45; EXPECT(limits_evaluate(&m, &c) == MODE_ERROR);
    REPORT();
}
