#include "test_util.h"
#include "config.h"
#include "protocol.h"
#include "bytes.h"

int main(void) {
    lnc_config_t c; config_load_defaults(&c);
    EXPECT(c.magic == LNC_CONFIG_MAGIC);

    /* serialize/deserialize roundtrip */
    c.temp_norm_lo = 11; c.batt_warn_lo = 2222;
    uint8_t buf[128];
    uint32_t n = config_serialize(&c, buf, sizeof(buf));
    EXPECT(n > 0);
    lnc_config_t back;
    EXPECT(config_deserialize(&back, buf, n));
    EXPECT(back.temp_norm_lo == 11 && back.batt_warn_lo == 2222);

    /* bad magic rejected */
    uint8_t zero[128] = {0};
    EXPECT(config_deserialize(&back, zero, sizeof(zero)) == false);

    /* apply each SET command tag */
    uint8_t v4[4]; bytes_put_i16(v4, 5); bytes_put_i16(v4+2, 35);
    EXPECT(config_apply_set(&c, CMD_SET_TEMP_NORMAL, v4, 4));
    EXPECT(c.temp_norm_lo == 5 && c.temp_norm_hi == 35);
    bytes_put_i16(v4, -10); bytes_put_i16(v4+2, 50);
    EXPECT(config_apply_set(&c, CMD_SET_TEMP_WARNING, v4, 4));
    EXPECT(c.temp_warn_lo == -10 && c.temp_warn_hi == 50);

    uint8_t v2[2];
    bytes_put_u16(v2, 55); EXPECT(config_apply_set(&c, CMD_SET_HUM_NORMAL, v2, 2));  EXPECT(c.hum_norm_lo==55);
    bytes_put_u16(v2, 25); EXPECT(config_apply_set(&c, CMD_SET_HUM_WARNING, v2, 2)); EXPECT(c.hum_warn_lo==25);
    bytes_put_u16(v2, 650);EXPECT(config_apply_set(&c, CMD_SET_LIGHT_NORMAL, v2, 2));EXPECT(c.light_norm_lo==650);
    bytes_put_u16(v2, 450);EXPECT(config_apply_set(&c, CMD_SET_LIGHT_WARNING, v2,2));EXPECT(c.light_warn_lo==450);
    bytes_put_u16(v2, 3100);EXPECT(config_apply_set(&c, CMD_SET_BATT_NORMAL, v2,2)); EXPECT(c.batt_norm_lo==3100);
    bytes_put_u16(v2, 2600);EXPECT(config_apply_set(&c, CMD_SET_BATT_WARNING, v2,2));EXPECT(c.batt_warn_lo==2600);

    /* wrong length and unknown tag rejected */
    EXPECT(config_apply_set(&c, CMD_SET_HUM_NORMAL, v2, 1) == false);
    EXPECT(config_apply_set(&c, 0x99, v2, 2) == false);
    REPORT();
}
