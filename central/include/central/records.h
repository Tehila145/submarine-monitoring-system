#pragma once
#include <cstdint>

namespace central {

// A measurement received from an LNC (from a keep-alive or a data report).
struct Measurement {
    uint32_t ts = 0;
    int16_t  temp = 0;
    uint16_t hum = 0, light = 0, batt = 0;
    uint8_t  mode = 0;   // 0 NORMAL, 1 WARNING, 2 ERROR
};

// An event received from an LNC.
struct EventRec {
    uint32_t ts = 0;
    uint8_t  src = 0;        // 0 MONITOR, 1 OBJECT, 2 CONFIG, 3 INIT
    uint8_t  from_mode = 0;  // for MONITOR transitions
    uint8_t  to_mode = 0;
    bool     detected = false; // for OBJECT events
};

}
