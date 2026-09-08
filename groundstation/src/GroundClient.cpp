#include "gs/GroundClient.h"
#include "central/ground_protocol.h"
#include "central/net.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace central;

namespace gs {

GroundClient::~GroundClient() { disconnect(); }

void GroundClient::disconnect() { if (fd_ >= 0) { ::close(fd_); fd_ = -1; } }

bool GroundClient::connectTo(const std::string& host, uint16_t port) {
    disconnect();
    fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd_ < 0) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) { disconnect(); return false; }
    if (::connect(fd_, (sockaddr*)&addr, sizeof(addr)) != 0)      { disconnect(); return false; }
    return true;
}

bool GroundClient::getLog(uint32_t from, uint32_t to, std::vector<Measurement>& out) {
    out.clear();
    if (fd_ < 0 || !netWriteFrame(fd_, gsReqLogRange(from, to))) return false;
    std::vector<uint8_t> f;
    for (;;) {
        if (!netReadFrame(fd_, f)) return false;
        uint32_t count; Measurement m;
        if (gsDecodeDone(f.data(), f.size(), count)) return out.size() == count;
        if (gsDecodeLog(f.data(), f.size(), m)) out.push_back(m);
    }
}

bool GroundClient::getEvents(uint32_t from, uint32_t to, std::vector<EventRec>& out) {
    out.clear();
    if (fd_ < 0 || !netWriteFrame(fd_, gsReqEventsRange(from, to))) return false;
    std::vector<uint8_t> f;
    for (;;) {
        if (!netReadFrame(fd_, f)) return false;
        uint32_t count; EventRec e;
        if (gsDecodeDone(f.data(), f.size(), count)) return out.size() == count;
        if (gsDecodeEvent(f.data(), f.size(), e)) out.push_back(e);
    }
}

}
