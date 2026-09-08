#include "central/Communication.h"
#include "central/codec.h"

namespace central {

static constexpr uint8_t MAXLEN = 48;

static bool isReportTag(uint8_t t) {
    return t == RPT_KEEPALIVE || t == RPT_EVENT || t == RPT_DATA || t == RSP_TIME;
}

// For nested reports, the value must be a run of child TLVs that exactly fills it.
static bool childrenOk(const uint8_t* v, uint8_t len) {
    uint32_t i = 0;
    while (i < len) {
        if (i + 2 > len) return false;
        uint8_t l = v[i + 1];
        if ((uint32_t)i + 2 + l > len) return false;
        i += 2 + l;
    }
    return i == len;
}

bool Communication::sendFrame(const std::vector<uint8_t>& frame) {
    if (frame.empty()) return false;
    return t_.send(frame.data(), frame.size());
}

void Communication::poll() {
    uint8_t tmp[256];
    std::size_t n = t_.recv(tmp, sizeof(tmp));
    if (n) buf_.insert(buf_.end(), tmp, tmp + n);

    // Extract as many complete, valid frames as possible.
    for (;;) {
        while (!buf_.empty() && !isReportTag(buf_[0])) buf_.erase(buf_.begin());
        if (buf_.size() < 2) break;
        uint8_t tag = buf_[0], length = buf_[1];
        if (length > MAXLEN) { buf_.erase(buf_.begin()); continue; }
        if (buf_.size() < 2u + length) break;                 // wait for the rest
        const uint8_t* val = buf_.data() + 2;
        if ((tag == RPT_KEEPALIVE || tag == RPT_EVENT || tag == RPT_DATA) &&
            !childrenOk(val, length)) {
            buf_.erase(buf_.begin()); continue;               // bad body — slide
        }
        Report r = parseReport(buf_.data(), 2u + length);
        if (handler_ && r.type != ReportType::None) handler_(r);
        buf_.erase(buf_.begin(), buf_.begin() + 2 + length);  // consume frame
    }
}

}
