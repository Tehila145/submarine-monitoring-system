// Central Computer — manages an LNC end unit over the binary TLV protocol.
// Modules: Communication (LNC-facing), Management Command, Log, Data Collection
// & Analysis. Speaks the same protocol as the firmware (shared tlv.c/protocol.h).
#include "central/SerialTransport.h"
#include "central/Communication.h"
#include "central/ManagementCommand.h"
#include "central/Database.h"
#include "central/GroundServer.h"
#include "central/Log.h"
#include <iostream>
#include <cstdio>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <glob.h>

using namespace central;

static std::string modeName(uint8_t m) { return m == 2 ? "ERROR" : (m == 1 ? "WARNING" : "NORMAL"); }
static std::string srcName(uint8_t s)  { const char* n[] = {"MONITOR","OBJECT","CONFIG","INIT"}; return s < 4 ? n[s] : "?"; }

static void pump(Communication& c, int ms) {
    auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        c.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
}

static std::string autodetect() {
    glob_t g; std::string p;
    if (glob("/dev/cu.usbmodem*", 0, nullptr, &g) == 0 && g.gl_pathc > 0) p = g.gl_pathv[0];
    globfree(&g);
    return p;
}

// Ground-facing server mode: the Central answers a Ground Station over Ethernet
// (TCP), serving logged data / events from its database (loaded from CSV).
//   central --serve [port=5555] [dbdir=.]
static int runGroundServer(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IONBF, 0);   // show connection/serve logs live
    uint16_t port = (argc > 2) ? (uint16_t)std::stoi(argv[2]) : 5555;
    std::string dir = (argc > 3) ? argv[3] : ".";
    Database db;
    db.loadCsv(dir);
    std::cout << "Central (Ground Station server) loaded " << db.measurements().size()
              << " measurements + " << db.events().size() << " events from " << dir << "\n";
    GroundServer gs(db);
    if (!gs.listenOn(port)) { std::cerr << "Cannot bind port " << port << "\n"; return 1; }
    std::cout << "Serving Ground Station on tcp://127.0.0.1:" << gs.port()
              << "  (Ctrl-C to stop)\n";
    gs.serveForever();
    return 0;
}

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--serve") return runGroundServer(argc, argv);

    std::string port = argc > 1 ? argv[1] : autodetect();
    if (port.empty()) { std::cerr << "No /dev/cu.usbmodem* found; pass the port.\n"; return 1; }
    SerialTransport serial(port);
    if (!serial.ok()) { std::cerr << "Cannot open " << port << "\n"; return 1; }

    Communication comm(serial);
    Database db;
    Log log("central.log");
    ManagementCommand mgmt(comm);
    int rxCount = 0;                 // frames seen (used to detect a silent Listen)

    comm.setHandler([&](const Report& r) {
        std::ostringstream o;
        if (r.type == ReportType::KeepAlive || r.type == ReportType::Data) {
            db.addMeasurement(r.m);
            o << (r.type == ReportType::KeepAlive ? "KEEP_ALIVE" : "DATA")
              << " ts=" << r.m.ts << " temp=" << r.m.temp << " hum=" << r.m.hum
              << " light=" << r.m.light << " batt=" << r.m.batt << " mode=" << modeName(r.m.mode);
        } else if (r.type == ReportType::Event) {
            db.addEvent(r.e);
            o << "EVENT      ts=" << r.e.ts << " src=" << srcName(r.e.src);
            if (r.e.src == 0) o << " " << modeName(r.e.from_mode) << "->" << modeName(r.e.to_mode);
            if (r.e.src == 1) o << (r.e.detected ? " DETECTED" : " cleared");
        } else if (r.type == ReportType::Time) {
            o << "TIME       " << r.time;
        }
        if (!o.str().empty()) { log.line(o.str()); ++rxCount; }
    });

    std::cout << "Central Computer on " << port << " @115200\n";
    serial.sendText("proto\r\n");     // flip the LNC into protocol mode
    pump(comm, 400);
    comm.poll();

    // Time sync (spec §2.7): push the host's current UTC time to the LNC on
    // connect, so its RTC reflects real wall-clock time with no manual setting.
    uint32_t nowEpoch = (uint32_t)std::time(nullptr);
    mgmt.setRtc(nowEpoch);
    std::cout << "Synced LNC clock to " << nowEpoch << " (host UTC time)\n";
    pump(comm, 400);

    const char* MENU =
        "\n=== Central Computer ===\n"
        " 1 Listen (watch keep-alive/events)   2 Get LNC time    3 Set LNC time\n"
        " 4 Set temp NORMAL range   5 Set temp WARNING range   6 Set light NORMAL\n"
        " 7 Retrieve data range     8 Retrieve events range\n"
        " 9 Report (analyse stored data)   10 Save database   11 Exit\n"
        "Choice: ";

    for (;;) {
        std::cout << MENU << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) break;
        std::istringstream in(line);
        int ch = 0; in >> ch;
        if (ch == 11) break;

        if (ch == 1) {
            int secs = 8; in >> secs;
            serial.sendText("proto\r\n");   // re-arm protocol mode in case the board reset to console
            pump(comm, 300);
            rxCount = 0;                    // count only frames from this listen window
            std::cout << "Listening " << secs << "s...\n";
            pump(comm, secs * 1000);
            if (rxCount == 0)
                std::cout << "(no frames received — the board is likely in console mode or was reset. "
                             "Press its RESET once, and make sure nothing else (e.g. screen) holds the serial port.)\n";
        } else if (ch == 2) {
            mgmt.getTime(); pump(comm, 1500);
        } else if (ch == 3) {
            unsigned long ep = 0; in >> ep; mgmt.setRtc((uint32_t)ep);
            std::cout << "Sent SET_RTC " << ep << "\n"; pump(comm, 800);
        } else if (ch == 4) {
            int lo, hi; if (in >> lo >> hi) { mgmt.setTempNormal((int16_t)lo, (int16_t)hi);
                std::cout << "Sent SET_TEMP_NORMAL " << lo << ".." << hi << "\n"; } pump(comm, 800);
        } else if (ch == 5) {
            int lo, hi; if (in >> lo >> hi) { mgmt.setTempWarning((int16_t)lo, (int16_t)hi);
                std::cout << "Sent SET_TEMP_WARNING " << lo << ".." << hi << "\n"; } pump(comm, 800);
        } else if (ch == 6) {
            int v; if (in >> v) { mgmt.setLightNormal((uint16_t)v);
                std::cout << "Sent SET_LIGHT_NORMAL " << v << "\n"; } pump(comm, 800);
        } else if (ch == 7) {
            unsigned long f = 0, t = 4000000000UL; in >> f >> t;
            std::cout << "Retrieving data " << f << ".." << t << " ...\n";
            mgmt.getDataRange((uint32_t)f, (uint32_t)t); pump(comm, 2500);
        } else if (ch == 8) {
            unsigned long f = 0, t = 4000000000UL; in >> f >> t;
            std::cout << "Retrieving events " << f << ".." << t << " ...\n";
            mgmt.getEventsRange((uint32_t)f, (uint32_t)t); pump(comm, 2500);
        } else if (ch == 9) {
            auto s = db.summarize(0, 0xFFFFFFFFu);
            auto e = db.eventStats(0, 0xFFFFFFFFu);
            std::cout << "\n-- Report (stored data) --\n";
            std::cout << "measurements: " << s.count << "\n";
            if (s.count) {
                std::cout << "  temp   min/avg/max = " << s.tmin << " / " << s.tavg << " / " << s.tmax << "\n";
                std::cout << "  hum    min/avg/max = " << s.hmin << " / " << s.havg << " / " << s.hmax << "\n";
                std::cout << "  light  min/avg/max = " << s.lmin << " / " << s.lavg << " / " << s.lmax << "\n";
                std::cout << "  batt   min/avg/max = " << s.bmin << " / " << s.bavg << " / " << s.bmax << "\n";
                std::cout << "  modes  NORMAL=" << s.modeNormal << " WARNING=" << s.modeWarning
                          << " ERROR=" << s.modeError << "\n";
            }
            std::cout << "events: " << e.total << "  (monitor=" << e.monitor << " object=" << e.object
                      << " [" << e.objectDetected << " detects] config=" << e.config << " init=" << e.init << ")\n";
        } else if (ch == 10) {
            std::cout << (db.saveCsv(".") ? "Saved measurements.csv + events.csv\n" : "Save failed\n");
        } else {
            std::cout << "Unknown option.\n";
        }
    }
    std::cout << "Bye (LNC stays in protocol mode; press its RESET for console).\n";
    return 0;
}
