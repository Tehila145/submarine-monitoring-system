#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include "central/records.h"

// Ground Station <-> Central Computer link (spec §1.2: "over Ethernet").
// These TLV tags are private to this link and independent of the LNC protocol,
// keeping the physical transport a detail of the communication module.
namespace central {

enum : uint8_t {
    // Ground Station -> Central (requests)
    GS_GET_LOG_RANGE    = 0x40,   // value: u32 from, u32 to
    GS_GET_EVENTS_RANGE = 0x41,   // value: u32 from, u32 to
    // Central -> Ground Station (responses)
    GS_LOG_RECORD       = 0x42,   // value: measurement (13 bytes)
    GS_EVENT_RECORD     = 0x43,   // value: event (8 bytes)
    GS_DONE             = 0x44,   // value: u32 count  (ends a response stream)
};

// --- Request builders (complete TLV frames [tag][len][value]) ---
std::vector<uint8_t> gsReqLogRange(uint32_t from, uint32_t to);
std::vector<uint8_t> gsReqEventsRange(uint32_t from, uint32_t to);

// --- Response builders ---
std::vector<uint8_t> gsLogRecord(const Measurement& m);
std::vector<uint8_t> gsEventRecord(const EventRec& e);
std::vector<uint8_t> gsDone(uint32_t count);

// --- Request parsing (Central side) ---
struct GsRequest { uint8_t tag = 0; uint32_t from = 0, to = 0; };
GsRequest gsParseRequest(const uint8_t* frame, std::size_t len);

// --- Response decoding (Ground Station side); return false on malformed input ---
bool gsDecodeLog(const uint8_t* frame, std::size_t len, Measurement& out);
bool gsDecodeEvent(const uint8_t* frame, std::size_t len, EventRec& out);
bool gsDecodeDone(const uint8_t* frame, std::size_t len, uint32_t& count);

}
