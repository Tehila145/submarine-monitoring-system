#pragma once
#include <cstdint>
#include <vector>
#include <functional>
#include "central/Transport.h"
#include "central/protocol_util.h"

namespace central {

// Communication module (spec §3.1): sends command frames to the LNC and runs a
// listener that reassembles incoming TLV frames and forwards each parsed report
// to a handler (which routes it to the Log / Data Collection modules).
class Communication {
public:
    using Handler = std::function<void(const Report&)>;

    explicit Communication(Transport& t) : t_(t) {}

    bool sendFrame(const std::vector<uint8_t>& frame);
    void setHandler(Handler h) { handler_ = std::move(h); }

    // Read whatever bytes are available, assemble complete frames, dispatch them.
    // Self-syncing: skips stray bytes. Call repeatedly (e.g. in a loop).
    void poll();

private:
    Transport&           t_;
    Handler              handler_;
    std::vector<uint8_t> buf_;   // reassembly buffer
};

}
