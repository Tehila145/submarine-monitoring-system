#include "test_util.h"
#include "central/Database.h"

using namespace central;

static Measurement mk(uint32_t ts, int16_t t, uint16_t h, uint16_t l, uint16_t b, uint8_t mode) {
    Measurement m; m.ts=ts; m.temp=t; m.hum=h; m.light=l; m.batt=b; m.mode=mode; return m;
}

int main() {
    Database db;
    db.addMeasurement(mk(1000, 20, 50, 700, 3000, 0));  // NORMAL
    db.addMeasurement(mk(1005, 30, 40, 500, 2500, 1));  // WARNING
    db.addMeasurement(mk(1010, 40, 30, 300, 2000, 2));  // ERROR

    EXPECT(db.measurements().size() == 3u);

    // range filter
    EXPECT(db.measurementsInRange(1000, 1005).size() == 2u);
    EXPECT(db.measurementsInRange(1006, 9999).size() == 1u);

    // summary
    auto s = db.summarize(0, 9999);
    EXPECT(s.count == 3u);
    EXPECT(s.tmin == 20 && s.tmax == 40);
    EXPECT(s.tavg == 30.0);
    EXPECT(s.bmin == 2000 && s.bmax == 3000);
    EXPECT(s.modeNormal == 1 && s.modeWarning == 1 && s.modeError == 1);

    // events + stats
    EventRec e1; e1.ts=1002; e1.src=0; e1.from_mode=0; e1.to_mode=1; db.addEvent(e1);
    EventRec e2; e2.ts=1004; e2.src=1; e2.detected=true; db.addEvent(e2);
    EventRec e3; e3.ts=1006; e3.src=3; db.addEvent(e3);
    auto es = db.eventStats(0, 9999);
    EXPECT(es.total == 3u && es.monitor == 1u && es.object == 1u &&
           es.init == 1u && es.objectDetected == 1u);

    // 7-day retention: a record 8 days older than the latest is pruned
    Database db2;
    db2.addMeasurement(mk(100, 20, 50, 700, 3000, 0));                    // old
    db2.addMeasurement(mk(100 + 8*24*3600, 21, 51, 701, 3001, 0));        // 8 days later
    EXPECT(db2.measurements().size() == 1u);           // old one dropped
    EXPECT(db2.measurements()[0].temp == 21);

    // persistence roundtrip
    db.saveCsv("/tmp");
    Database db3; db3.loadCsv("/tmp");
    EXPECT(db3.measurements().size() == 3u);
    EXPECT(db3.events().size() == 3u);
    EXPECT(db3.summarize(0,9999).tmax == 40);
    REPORT();
}
