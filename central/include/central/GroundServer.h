#pragma once
#include <cstdint>
#include "central/Database.h"

namespace central {

// Ground-facing side of the Communication module (spec §1.2, §4): a TCP server
// that accepts a Ground Station connection over Ethernet, answers requests for
// logged data / events over a time range, and streams the matching records back.
class GroundServer {
public:
    explicit GroundServer(Database& db) : db_(db) {}
    ~GroundServer();

    bool     listenOn(uint16_t port);          // bind + listen; port 0 = ephemeral. false on error
    uint16_t port() const { return port_; }    // actual bound port (useful with port 0)

    void serveForever(bool verbose = true);     // accept loop, blocking (server mode)
    bool serveOne();                            // accept one client, serve until it disconnects (tests)

private:
    void handleClient(int cfd, bool verbose);
    Database& db_;
    int      lfd_  = -1;
    uint16_t port_ = 0;
};

}
