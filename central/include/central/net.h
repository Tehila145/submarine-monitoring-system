#pragma once
// Minimal blocking TLV framing over a connected socket fd, shared by the
// Ground Station client and the Central's Ground-facing server. A frame is
// [tag][len][value...]; TCP is a byte stream, so we read the 2-byte header
// then exactly `len` value bytes.
#include <cstdint>
#include <cstddef>
#include <vector>
#include <cerrno>
#include <unistd.h>

namespace central {

inline bool netWriteAll(int fd, const uint8_t* p, std::size_t n) {
    std::size_t off = 0;
    while (off < n) {
        ssize_t w = ::write(fd, p + off, n - off);
        if (w > 0) off += (std::size_t)w;
        else if (w < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        else return false;
    }
    return true;
}

inline bool netWriteFrame(int fd, const std::vector<uint8_t>& f) {
    return !f.empty() && netWriteAll(fd, f.data(), f.size());
}

inline bool netReadExact(int fd, uint8_t* buf, std::size_t n) {
    std::size_t off = 0;
    while (off < n) {
        ssize_t r = ::read(fd, buf + off, n - off);
        if (r > 0) off += (std::size_t)r;
        else if (r == 0) return false;                        // peer closed
        else if (r < 0 && (errno == EINTR || errno == EAGAIN)) continue;
        else return false;
    }
    return true;
}

// Read one complete TLV frame into `out`. Returns false on EOF or error.
inline bool netReadFrame(int fd, std::vector<uint8_t>& out) {
    uint8_t hdr[2];
    if (!netReadExact(fd, hdr, 2)) return false;
    uint8_t len = hdr[1];
    out.resize(2u + len);
    out[0] = hdr[0]; out[1] = hdr[1];
    if (len && !netReadExact(fd, out.data() + 2, len)) return false;
    return true;
}

}
