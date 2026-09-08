#pragma once
#include <cstdint>
#include "central/Communication.h"
#include "central/protocol_util.h"
#include "central/codec.h"

namespace central {

// Management Command module (spec §3.2): builds management commands and sends
// them to the LNC via the Communication module.
class ManagementCommand {
public:
    explicit ManagementCommand(Communication& c) : c_(c) {}

    bool setTempNormal(int16_t lo, int16_t hi)  { return c_.sendFrame(cmdSetTempNormal(lo, hi)); }
    bool setTempWarning(int16_t lo, int16_t hi) { return c_.sendFrame(cmdSetTempWarning(lo, hi)); }
    bool setHumidityNormal(uint16_t v)  { return c_.sendFrame(cmdSetLowerBound(CMD_SET_HUM_NORMAL, v)); }
    bool setHumidityWarning(uint16_t v) { return c_.sendFrame(cmdSetLowerBound(CMD_SET_HUM_WARNING, v)); }
    bool setLightNormal(uint16_t v)     { return c_.sendFrame(cmdSetLowerBound(CMD_SET_LIGHT_NORMAL, v)); }
    bool setLightWarning(uint16_t v)    { return c_.sendFrame(cmdSetLowerBound(CMD_SET_LIGHT_WARNING, v)); }
    bool setBatteryNormal(uint16_t v)   { return c_.sendFrame(cmdSetLowerBound(CMD_SET_BATT_NORMAL, v)); }
    bool setBatteryWarning(uint16_t v)  { return c_.sendFrame(cmdSetLowerBound(CMD_SET_BATT_WARNING, v)); }
    bool setRtc(uint32_t epoch)         { return c_.sendFrame(cmdSetRtc(epoch)); }
    bool getTime()                      { return c_.sendFrame(cmdGetTime()); }
    bool getDataRange(uint32_t f, uint32_t t)   { return c_.sendFrame(cmdGetDataRange(f, t)); }
    bool getEventsRange(uint32_t f, uint32_t t) { return c_.sendFrame(cmdGetEventsRange(f, t)); }

private:
    Communication& c_;
};

}
