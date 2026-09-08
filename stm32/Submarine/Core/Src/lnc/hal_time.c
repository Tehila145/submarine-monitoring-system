#include "hal_time.h"
#include "board.h"   /* hrtc, HAL */
#include <stdio.h>

/* Epoch<->civil date (Howard Hinnant's algorithms), base 1970-01-01.
 * STM32 RTC stores a two-digit year within the 2000s. */
static long days_from_civil(int y, unsigned m, unsigned d) {
    y -= (m <= 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097L + (long)doe - 719468L;
}

static void civil_from_days(long z, int* y, unsigned* m, unsigned* d) {
    z += 719468L;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int yy = (int)yoe + (int)(era * 400);
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    *y = yy + (*m <= 2);
}

uint32_t rtc_now(void) {
    RTC_TimeTypeDef t; RTC_DateTypeDef d;
    /* HAL requires reading time before date to unlock the shadow registers. */
    HAL_RTC_GetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &d, RTC_FORMAT_BIN);
    long days = days_from_civil(2000 + d.Year, d.Month, d.Date);
    return (uint32_t)(days * 86400L + t.Hours * 3600 + t.Minutes * 60 + t.Seconds);
}

void rtc_set(uint32_t epoch) {
    long days = (long)(epoch / 86400u);
    uint32_t rem = epoch % 86400u;
    int y; unsigned m, dd;
    civil_from_days(days, &y, &m, &dd);
    RTC_DateTypeDef d = {0};
    d.Year = (uint8_t)(y - 2000);
    d.Month = (uint8_t)m;
    d.Date = (uint8_t)dd;
    d.WeekDay = RTC_WEEKDAY_MONDAY;   /* weekday unused by the LNC */
    RTC_TimeTypeDef t = {0};
    t.Hours = (uint8_t)(rem / 3600);
    t.Minutes = (uint8_t)((rem % 3600) / 60);
    t.Seconds = (uint8_t)(rem % 60);
    HAL_RTC_SetTime(&hrtc, &t, RTC_FORMAT_BIN);
    HAL_RTC_SetDate(&hrtc, &d, RTC_FORMAT_BIN);
}

void rtc_datestr8(char out[9]) {
    uint32_t epoch = rtc_now();
    long days = (long)(epoch / 86400u);
    int y; unsigned m, d;
    civil_from_days(days, &y, &m, &d);
    snprintf(out, 9, "%04d%02u%02u", y, m, d);
}
