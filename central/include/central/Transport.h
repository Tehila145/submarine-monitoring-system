#pragma once
#include <cstdint>
#include <cstddef>

namespace central {

// Transport-independent link to the LNC (spec §3.1): UART today, Ethernet
// tomorrow — only the concrete subclass changes. recv() is non-blocking.
class Transport {
public:
    virtual ~Transport() = default;
    virtual bool        send(const uint8_t* data, std::size_t len) = 0;
    virtual std::size_t recv(uint8_t* buf, std::size_t cap) = 0;   // bytes read, 0 if none
};

}
