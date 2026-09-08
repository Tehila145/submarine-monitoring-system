#include "test_util.h"
#include "central/Communication.h"
#include "central/codec.h"
#include <vector>
#include <deque>

using namespace central;

struct FakeTransport : Transport {
    std::deque<uint8_t> in;
    std::vector<uint8_t> out;
    bool send(const uint8_t* d, std::size_t n) override { out.insert(out.end(), d, d + n); return true; }
    std::size_t recv(uint8_t* b, std::size_t cap) override {
        std::size_t k = 0; while (k < cap && !in.empty()) { b[k++] = in.front(); in.pop_front(); } return k;
    }
    void feed(const std::vector<uint8_t>& v) { in.insert(in.end(), v.begin(), v.end()); }
};

static std::vector<uint8_t> keepAlive(uint32_t ts, int16_t t, uint16_t h,
                                      uint16_t l, uint16_t b, uint8_t mode) {
    uint8_t inner[48]; int off = 0, n;
    uint8_t tsb[4]; bytes_put_u32(tsb, ts);
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_TIMESTAMP, tsb, 4); off += n;
    uint8_t meas[8]; bytes_put_i16(meas, t); bytes_put_u16(meas + 2, h);
    bytes_put_u16(meas + 4, l); bytes_put_u16(meas + 6, b);
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_MEASUREMENT, meas, 8); off += n;
    uint8_t md = mode;
    n = tlv_write(inner + off, (uint32_t)(sizeof(inner) - off), TAG_MODE, &md, 1); off += n;
    std::vector<uint8_t> f(off + 2);
    tlv_write(f.data(), (uint32_t)f.size(), RPT_KEEPALIVE, inner, (uint8_t)off);
    return f;
}

int main() {
    FakeTransport ft;
    Communication comm(ft);
    std::vector<Report> got;
    comm.setHandler([&](const Report& r){ got.push_back(r); });

    // garbage prefix + a keep-alive -> one dispatched report
    ft.feed({0x99, 0x99});
    ft.feed(keepAlive(946684817u, 25, 54, 700, 3300, 1));
    comm.poll();
    EXPECT(got.size() == 1u);
    EXPECT(got[0].type == ReportType::KeepAlive && got[0].m.temp == 25 && got[0].m.mode == 1);

    // a frame split across two polls
    got.clear();
    auto ka2 = keepAlive(2, 10, 20, 30, 40, 0);
    std::vector<uint8_t> first(ka2.begin(), ka2.begin() + 3), rest(ka2.begin() + 3, ka2.end());
    ft.feed(first); comm.poll(); EXPECT(got.empty());
    ft.feed(rest);  comm.poll(); EXPECT(got.size() == 1u && got[0].m.temp == 10);

    // sending a command frame reaches the transport
    comm.sendFrame(cmdGetTime());
    EXPECT(ft.out.size() == 2u && ft.out[0] == CMD_GET_TIME);
    REPORT();
}
