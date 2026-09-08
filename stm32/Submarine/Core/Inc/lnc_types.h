#ifndef LNC_TYPES_H
#define LNC_TYPES_H
#include <stdint.h>
#include <stdbool.h>

typedef enum { MODE_NORMAL = 0, MODE_WARNING = 1, MODE_ERROR = 2 } lnc_mode_t;
typedef enum { LED_GREEN = 0, LED_YELLOW = 1, LED_RED = 2 } led_color_t;
typedef enum { SRC_MONITOR = 0, SRC_OBJECT, SRC_CONFIG, SRC_INIT } evt_src_t;
typedef enum { RESET_NORMAL = 0, RESET_WATCHDOG = 1 } reset_cause_t;

typedef struct {
    uint32_t magic;                        /* sentinel: valid vs first boot */
    int16_t  temp_norm_lo, temp_norm_hi;   /* temperature: a RANGE */
    int16_t  temp_warn_lo, temp_warn_hi;
    uint16_t hum_norm_lo,  hum_warn_lo;    /* others: LOWER boundaries */
    uint16_t light_norm_lo, light_warn_lo;
    uint16_t batt_norm_lo, batt_warn_lo;
} lnc_config_t;

typedef struct {
    uint32_t   ts;
    int16_t    temperature;
    uint16_t   humidity;
    uint16_t   light;
    uint16_t   battery;
    lnc_mode_t mode;
} measurement_t;

typedef struct {
    evt_src_t src;
    uint32_t  ts;
    union {
        struct { lnc_mode_t from, to; measurement_t m; } transition; /* SRC_MONITOR */
        struct { bool detected; } object;                            /* SRC_OBJECT  */
        struct { bool after_wd_reset; } startup;                     /* SRC_INIT    */
    } data;
} lnc_event_t;

#endif
