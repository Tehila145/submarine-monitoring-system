#include "test_util.h"
#include <sstream>
#include "fleet/Submarine.h"
#include "fleet/Message.h"

using namespace fleet;

namespace {
// Minimal concrete double to exercise the abstract base.
class StubSub : public Submarine {
public:
    using Submarine::Submarine;
    std::string typeName() const override { return "Stub"; }
    void display(std::ostream& out) const override { printHeader(out); out << "\n"; }
    void updateMissionDetails(std::istream& in, std::ostream&) override {
        std::getline(in, detail);
    }
    std::string detail;
protected:
    void clearMissionDetails() override { detail.clear(); }
};
}

int main() {
    StubSub s("R-001", "Poseidon");
    EXPECT(s.serial() == "R-001");
    EXPECT(s.name() == "Poseidon");
    EXPECT(s.isAvailable());

    std::istringstream in("survey the trench\n");
    std::ostringstream out;
    s.assignMission(in, out);
    EXPECT(!s.isAvailable());
    EXPECT(s.detail == "survey the trench");
    s.endMission();
    EXPECT(s.isAvailable());
    EXPECT(s.detail == "");

    StubSub a("C-001", "Nautilus");
    StubSub b("C-002", "Triton");
    a.receiveMessage(Message("first", &b));
    a.receiveMessage(Message("second", &b));
    std::ostringstream o2;
    a.displayMessages(o2);
    EXPECT(o2.str() ==
        "Messages for C-001 (2):\n"
        "  from C-002: first\n"
        "  from C-002: second\n");

    StubSub c("C-003", "Seawolf");
    std::ostringstream o3;
    c.displayMessages(o3);
    EXPECT(o3.str() == "Messages for C-003: none\n");
    REPORT();
}
