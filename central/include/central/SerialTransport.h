#pragma once
#include <string>
#include "central/Transport.h"

namespace central {

// UART implementation of the transport interface (macOS/POSIX termios).
// Swapping to Ethernet = a different Transport subclass; nothing else changes.
class SerialTransport : public Transport {
public:
    explicit SerialTransport(const std::string& port);
    ~SerialTransport() override;

    bool ok() const { return fd_ >= 0; }
    bool        send(const uint8_t* data, std::size_t len) override;
    std::size_t recv(uint8_t* buf, std::size_t cap) override;
    void        sendText(const char* s);   // e.g. the ASCII 'proto\r\n' flip

private:
    int fd_ = -1;
};

}
