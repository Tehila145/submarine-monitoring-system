#include "test_util.h"
#include "logline.h"
#include <string.h>

int main(void) {
    /* measurement roundtrip */
    measurement_t m = { .ts=1000, .temperature=-5, .humidity=50,
                        .light=600, .battery=3200, .mode=MODE_WARNING };
    char line[64];
    int n = logline_format_measurement(line, sizeof(line), &m);
    EXPECT(n > 0);
    measurement_t mb;
    EXPECT(logline_parse_measurement(line, &mb));
    EXPECT(mb.ts==1000u && mb.temperature==-5 && mb.humidity==50 &&
           mb.light==600 && mb.battery==3200 && mb.mode==MODE_WARNING);

    /* monitor-transition event roundtrip */
    lnc_event_t e = {0}; e.src=SRC_MONITOR; e.ts=2000;
    e.data.transition.from=MODE_NORMAL; e.data.transition.to=MODE_ERROR;
    n = logline_format_event(line, sizeof(line), &e);
    EXPECT(n > 0);
    lnc_event_t eb;
    EXPECT(logline_parse_event(line, &eb));
    EXPECT(eb.src==SRC_MONITOR && eb.ts==2000u &&
           eb.data.transition.from==MODE_NORMAL && eb.data.transition.to==MODE_ERROR);

    /* object event roundtrip */
    lnc_event_t o = {0}; o.src=SRC_OBJECT; o.ts=3000; o.data.object.detected=true;
    EXPECT(logline_format_event(line, sizeof(line), &o) > 0);
    EXPECT(logline_parse_event(line, &eb));
    EXPECT(eb.src==SRC_OBJECT && eb.data.object.detected==true);

    /* startup event roundtrip */
    lnc_event_t s = {0}; s.src=SRC_INIT; s.ts=4000; s.data.startup.after_wd_reset=true;
    EXPECT(logline_format_event(line, sizeof(line), &s) > 0);
    EXPECT(logline_parse_event(line, &eb));
    EXPECT(eb.src==SRC_INIT && eb.data.startup.after_wd_reset==true);

    /* malformed line rejected */
    EXPECT(logline_parse_measurement("garbage", &mb) == false);
    REPORT();
}
