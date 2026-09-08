// Ground Station (spec §4): connects to the submarine's Central Computer over
// Ethernet (TCP) and requests data / events stored over a period of time.
//   groundstation [host=127.0.0.1] [port=5555]
#include "gs/GroundClient.h"
#include "central/records.h"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>

using namespace central;
using gs::GroundClient;

static std::string modeName(uint8_t m) { return m == 2 ? "ERROR" : (m == 1 ? "WARNING" : "NORMAL"); }
static std::string srcName(uint8_t s)  { const char* n[] = {"MONITOR","OBJECT","CONFIG","INIT"}; return s < 4 ? n[s] : "?"; }

static std::string when(uint32_t ts) {
    std::time_t t = (std::time_t)ts;
    std::tm* g = std::localtime(&t);   // show the viewer's local wall-clock time
    char b[24];
    if (g && std::strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", g)) return b;
    return std::to_string(ts);
}

// Read "from to" from the rest of the menu line, or prompt. Defaults: all time.
static void readRange(std::istringstream& in, uint32_t& from, uint32_t& to) {
    unsigned long f = 0, t = 4294967295UL;
    if (!(in >> f)) {
        std::cout << "  from (epoch, blank=0): ";
        std::string s; std::getline(std::cin, s);
        std::istringstream is(s); if (!(is >> f)) f = 0;
        std::cout << "  to   (epoch, blank=now/all): ";
        std::getline(std::cin, s); std::istringstream is2(s); if (!(is2 >> t)) t = 4294967295UL;
    } else { in >> t; }
    from = (uint32_t)f; to = (uint32_t)t;
}

int main(int argc, char** argv) {
    std::string host = argc > 1 ? argv[1] : "127.0.0.1";
    uint16_t    port = argc > 2 ? (uint16_t)std::stoi(argv[2]) : 5555;

    GroundClient client;
    if (!client.connectTo(host, port)) {
        std::cerr << "Cannot connect to Central at " << host << ":" << port
                  << "  (start it with:  ./central --serve " << port << " <dbdir>)\n";
        return 1;
    }
    std::cout << "Ground Station connected to Central at " << host << ":" << port << "\n";

    const char* MENU =
        "\n=== Ground Station ===\n"
        " 1 Retrieve log data (measurements) for a time range\n"
        " 2 Retrieve events for a time range\n"
        " 3 Exit\n"
        "Choice: ";

    for (;;) {
        std::cout << MENU << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) break;
        std::istringstream in(line);
        int ch = 0; in >> ch;
        if (ch == 3) break;

        if (ch == 1) {
            uint32_t from, to; readRange(in, from, to);
            std::vector<Measurement> rows;
            if (!client.getLog(from, to, rows)) { std::cerr << "Request failed / disconnected.\n"; break; }
            std::cout << "\n-- Log data " << from << ".." << to << "  (" << rows.size() << " records) --\n";
            std::cout << "  timestamp (UTC)        temp   hum  light  batt   mode\n";
            for (const auto& m : rows)
                std::printf("  %-19s  %5d %5u %6u %5u   %s\n",
                            when(m.ts).c_str(), m.temp, m.hum, m.light, m.batt, modeName(m.mode).c_str());
        } else if (ch == 2) {
            uint32_t from, to; readRange(in, from, to);
            std::vector<EventRec> rows;
            if (!client.getEvents(from, to, rows)) { std::cerr << "Request failed / disconnected.\n"; break; }
            std::cout << "\n-- Events " << from << ".." << to << "  (" << rows.size() << " records) --\n";
            for (const auto& e : rows) {
                std::cout << "  " << when(e.ts) << "  " << srcName(e.src);
                if (e.src == 0) std::cout << "  " << modeName(e.from_mode) << "->" << modeName(e.to_mode);
                if (e.src == 1) std::cout << (e.detected ? "  DETECTED" : "  cleared");
                std::cout << "\n";
            }
        } else {
            std::cout << "Unknown option.\n";
        }
    }
    std::cout << "Ground Station closing.\n";
    return 0;
}
