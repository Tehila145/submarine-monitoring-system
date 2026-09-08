#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "central/records.h"

// Ground Station client: connects to the Central Computer over TCP (Ethernet)
// and requests logged data / events for a time range (spec §4).
namespace gs {

class GroundClient {
public:
    ~GroundClient();
    bool connectTo(const std::string& host, uint16_t port);   // false on error
    bool connected() const { return fd_ >= 0; }
    void disconnect();

    // Send a request and collect the streamed response until GS_DONE.
    // Return false on I/O error; `out` holds the received records on success.
    bool getLog(uint32_t from, uint32_t to, std::vector<central::Measurement>& out);
    bool getEvents(uint32_t from, uint32_t to, std::vector<central::EventRec>& out);

private:
    int fd_ = -1;
};

}
