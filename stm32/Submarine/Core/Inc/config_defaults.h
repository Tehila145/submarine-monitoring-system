#ifndef CONFIG_DEFAULTS_H
#define CONFIG_DEFAULTS_H
/* First-boot default limits. TODO(team): tune these to your actual sensors and
 * units once temp/humidity (DHT) and battery/light (ADC) are wired. The spec
 * (§2.6) requires defaults to exist but does not fix the values. The values
 * below keep the stubbed in-range sensor readings in Normal mode so the board
 * boots green during bring-up. */
/* Demo-tuned so a warm breath on the DHT walks the modes:
 *   <=28C NORMAL, 29..33C WARNING, >=34C ERROR (temperature is a range).
 * Humidity/light/battery kept permissive so only temperature drives the demo. */
#define DEF_TEMP_NORM_LO   10
#define DEF_TEMP_NORM_HI   28
#define DEF_TEMP_WARN_LO   5
#define DEF_TEMP_WARN_HI   33
#define DEF_HUM_NORM_LO    20
#define DEF_HUM_WARN_LO    10
/* Light (LDR, 12-bit): bright~4095, hand-cover~1255. Cover -> WARNING, dark -> ERROR. */
#define DEF_LIGHT_NORM_LO  2000
#define DEF_LIGHT_WARN_LO  1000
/* Battery (pot, 12-bit): turn down past these to trip WARNING then ERROR. */
#define DEF_BATT_NORM_LO   2000
#define DEF_BATT_WARN_LO   1000
#endif
