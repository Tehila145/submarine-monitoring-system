#include "central/SerialTransport.h"
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <cstring>

namespace central {

SerialTransport::SerialTransport(const std::string& port) {
    fd_ = ::open(port.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd_ < 0) return;
    // Set 115200 raw ON the open fd (macOS resets the port to 9600 on open).
    struct termios a;
    if (tcgetattr(fd_, &a) != 0) { ::close(fd_); fd_ = -1; return; }
    cfmakeraw(&a);
    a.c_cflag |= (CREAD | CLOCAL | CS8);
    a.c_cc[VMIN] = 0;
    a.c_cc[VTIME] = 0;
    cfsetispeed(&a, B115200);
    cfsetospeed(&a, B115200);
    tcsetattr(fd_, TCSANOW, &a);
}

SerialTransport::~SerialTransport() {
    if (fd_ >= 0) ::close(fd_);
}

bool SerialTransport::send(const uint8_t* data, std::size_t len) {
    if (fd_ < 0) return false;
    std::size_t off = 0;
    while (off < len) {
        ssize_t w = ::write(fd_, data + off, len - off);
        if (w > 0) off += (std::size_t)w;
        else if (w < 0 && errno == EAGAIN) continue;   // tx buffer full, retry
        else return false;
    }
    return true;
}

std::size_t SerialTransport::recv(uint8_t* buf, std::size_t cap) {
    if (fd_ < 0) return 0;
    ssize_t r = ::read(fd_, buf, cap);
    return (r > 0) ? (std::size_t)r : 0;               // 0 on EAGAIN / no data
}

void SerialTransport::sendText(const char* s) {
    send(reinterpret_cast<const uint8_t*>(s), std::strlen(s));
}

}
