// Ground Station <-> Central protocol: codec round-trips + a real TCP loopback
// against GroundServer (spec §4).
#include "central/ground_protocol.h"
#include "central/GroundServer.h"
#include "central/net.h"
#include "central/Database.h"
#include "central/records.h"
#include "test_util.h"
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace central;

static void codecRoundTrips() {
    // Requests
    { auto f = gsReqLogRange(100, 250); auto r = gsParseRequest(f.data(), f.size());
      EXPECT(r.tag == GS_GET_LOG_RANGE); EXPECT(r.from == 100); EXPECT(r.to == 250); }
    { auto f = gsReqEventsRange(7, 9999); auto r = gsParseRequest(f.data(), f.size());
      EXPECT(r.tag == GS_GET_EVENTS_RANGE); EXPECT(r.from == 7); EXPECT(r.to == 9999); }

    // Measurement record (incl. negative temp + ERROR mode)
    { Measurement m; m.ts = 1710000000u; m.temp = -123; m.hum = 55; m.light = 700; m.batt = 3300; m.mode = 2;
      auto f = gsLogRecord(m); Measurement o;
      EXPECT(gsDecodeLog(f.data(), f.size(), o));
      EXPECT(o.ts==m.ts && o.temp==m.temp && o.hum==m.hum && o.light==m.light && o.batt==m.batt && o.mode==m.mode); }

    // Event records: monitor transition + object detected/cleared
    { EventRec e; e.ts = 42; e.src = 0; e.from_mode = 0; e.to_mode = 1;
      auto f = gsEventRecord(e); EventRec o;
      EXPECT(gsDecodeEvent(f.data(), f.size(), o));
      EXPECT(o.ts==42 && o.src==0 && o.from_mode==0 && o.to_mode==1); }
    { EventRec e; e.ts = 43; e.src = 1; e.detected = true;
      auto f = gsEventRecord(e); EventRec o;
      EXPECT(gsDecodeEvent(f.data(), f.size(), o));
      EXPECT(o.src==1 && o.detected==true); }

    // Done marker
    { auto f = gsDone(17); uint32_t c = 0; EXPECT(gsDecodeDone(f.data(), f.size(), c)); EXPECT(c == 17); }

    // Cross-tag rejection
    { auto f = gsDone(1); Measurement o; EXPECT(!gsDecodeLog(f.data(), f.size(), o)); }
}

static void loopback() {
    Database db;
    for (uint32_t t = 100; t <= 105; ++t) { Measurement m; m.ts = t; m.temp = (int16_t)t; m.mode = 0; db.addMeasurement(m); }
    { EventRec e; e.ts = 102; e.src = 1; e.detected = true; db.addEvent(e); }
    { EventRec e; e.ts = 104; e.src = 0; e.from_mode = 0; e.to_mode = 2; db.addEvent(e); }

    GroundServer srv(db);
    EXPECT(srv.listenOn(0));               // ephemeral port
    uint16_t p = srv.port();
    EXPECT(p != 0);

    std::thread th([&]{ srv.serveOne(); });

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    EXPECT(fd >= 0);
    sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(p);
    ::inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
    EXPECT(::connect(fd, (sockaddr*)&a, sizeof(a)) == 0);

    // Ask for log 101..104 -> expect 4 records (101,102,103,104)
    EXPECT(netWriteFrame(fd, gsReqLogRange(101, 104)));
    std::vector<Measurement> got; std::vector<uint8_t> f; uint32_t cnt = 0;
    for (;;) { EXPECT(netReadFrame(fd, f)); Measurement m;
        if (gsDecodeDone(f.data(), f.size(), cnt)) break;
        if (gsDecodeLog(f.data(), f.size(), m)) got.push_back(m); }
    EXPECT(got.size() == 4); EXPECT(cnt == 4);
    if (got.size() == 4) { EXPECT(got.front().ts == 101); EXPECT(got.back().ts == 104); }

    // Ask for events (all) -> expect 2
    EXPECT(netWriteFrame(fd, gsReqEventsRange(0, 0xFFFFFFFFu)));
    std::vector<EventRec> ev; uint32_t ec = 0;
    for (;;) { EXPECT(netReadFrame(fd, f)); EventRec e;
        if (gsDecodeDone(f.data(), f.size(), ec)) break;
        if (gsDecodeEvent(f.data(), f.size(), e)) ev.push_back(e); }
    EXPECT(ev.size() == 2); EXPECT(ec == 2);

    ::close(fd);       // let the server's serveOne() return
    th.join();
}

int main() {
    codecRoundTrips();
    loopback();
    REPORT();
}
