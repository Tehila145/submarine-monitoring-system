#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "central/records.h"

namespace central {

enum class ReportType { None, KeepAlive, Data, Event, Time };

struct Report {
    ReportType  type = ReportType::None;
    Measurement m{};
    EventRec    e{};
    uint32_t    time = 0;   // for RSP_TIME
};

// --- Management command builders (return a complete TLV frame) ---
std::vector<uint8_t> cmdSetTempNormal(int16_t lo, int16_t hi);
std::vector<uint8_t> cmdSetTempWarning(int16_t lo, int16_t hi);
std::vector<uint8_t> cmdSetLowerBound(uint8_t cmdTag, uint16_t v);  // hum/light/batt normal/warning
std::vector<uint8_t> cmdSetRtc(uint32_t epoch);
std::vector<uint8_t> cmdGetTime();
std::vector<uint8_t> cmdGetDataRange(uint32_t from, uint32_t to);
std::vector<uint8_t> cmdGetEventsRange(uint32_t from, uint32_t to);

// --- Parse one framed report from the LNC ([tag][len][value]) ---
Report parseReport(const uint8_t* frame, std::size_t len);

}
