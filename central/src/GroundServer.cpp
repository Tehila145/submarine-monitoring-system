#include "central/GroundServer.h"
#include "central/ground_protocol.h"
#include "central/net.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>

namespace central {

GroundServer::~GroundServer() { if (lfd_ >= 0) ::close(lfd_); }

bool GroundServer::listenOn(uint16_t port) {
    lfd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (lfd_ < 0) return false;
    int yes = 1;
    ::setsockopt(lfd_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   // localhost only
    addr.sin_port = htons(port);
    if (::bind(lfd_, (sockaddr*)&addr, sizeof(addr)) != 0) { ::close(lfd_); lfd_ = -1; return false; }
    if (::listen(lfd_, 4) != 0)                       { ::close(lfd_); lfd_ = -1; return false; }

    socklen_t sl = sizeof(addr);
    if (::getsockname(lfd_, (sockaddr*)&addr, &sl) == 0) port_ = ntohs(addr.sin_port);
    return true;
}

void GroundServer::handleClient(int cfd, bool verbose) {
    std::vector<uint8_t> frame;
    while (netReadFrame(cfd, frame)) {
        GsRequest req = gsParseRequest(frame.data(), frame.size());
        if (req.tag == GS_GET_LOG_RANGE) {
            auto rows = db_.measurementsInRange(req.from, req.to);
            if (verbose) std::printf("[ground] LOG %u..%u -> %zu records\n", req.from, req.to, rows.size());
            for (const auto& m : rows) if (!netWriteFrame(cfd, gsLogRecord(m))) return;
            if (!netWriteFrame(cfd, gsDone((uint32_t)rows.size()))) return;
        } else if (req.tag == GS_GET_EVENTS_RANGE) {
            auto rows = db_.eventsInRange(req.from, req.to);
            if (verbose) std::printf("[ground] EVENTS %u..%u -> %zu records\n", req.from, req.to, rows.size());
            for (const auto& e : rows) if (!netWriteFrame(cfd, gsEventRecord(e))) return;
            if (!netWriteFrame(cfd, gsDone((uint32_t)rows.size()))) return;
        } else {
            if (verbose) std::printf("[ground] ignoring unknown request tag 0x%02X\n",
                                     frame.empty() ? 0 : frame[0]);
        }
    }
}

void GroundServer::serveForever(bool verbose) {
    if (lfd_ < 0) return;
    for (;;) {
        sockaddr_in peer{}; socklen_t pl = sizeof(peer);
        int cfd = ::accept(lfd_, (sockaddr*)&peer, &pl);
        if (cfd < 0) { if (errno == EINTR) continue; return; }
        if (verbose) std::printf("[ground] connection from %s:%u\n",
                                 inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
        handleClient(cfd, verbose);
        ::close(cfd);
        if (verbose) std::printf("[ground] client disconnected\n");
    }
}

bool GroundServer::serveOne() {
    if (lfd_ < 0) return false;
    int cfd = ::accept(lfd_, nullptr, nullptr);
    if (cfd < 0) return false;
    handleClient(cfd, /*verbose=*/false);
    ::close(cfd);
    return true;
}

}
