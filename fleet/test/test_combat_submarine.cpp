#include "test_util.h"
#include <sstream>
#include "fleet/CombatSubmarine.h"

using namespace fleet;

int main() {
    CombatSubmarine c("C-001", "Nautilus");
    std::istringstream in("Patrol sector 7\nCmdr Yael Bar\n42\n");
    std::ostringstream sink;
    c.assignMission(in, sink);
    std::ostringstream out;
    c.display(out);
    EXPECT(out.str() ==
        "[Combat] Serial: C-001 | Name: Nautilus | Status: On mission\n"
        "  Mission: Patrol sector 7\n"
        "  Commander: Cmdr Yael Bar | Personnel: 42\n"
        "  Partners: \n");

    EXPECT(c.centralComputer().status() == "operational");

    CombatSubmarine a("C-010", "Alpha");
    CombatSubmarine b("C-011", "Bravo");
    a.addPartner(&b);
    a.addPartner(&b);   // duplicate ignored
    a.addPartner(&a);   // self ignored
    EXPECT(a.partners().size() == 1u);
    EXPECT(b.partners().size() == 1u);
    EXPECT(a.partners()[0] == &b);
    EXPECT(b.partners()[0] == &a);
    EXPECT(a.sharesMissionWith(&b));
    EXPECT(!a.sharesMissionWith(&a));

    std::istringstream in2("Patrol\nCmdr X\n10\n");
    std::ostringstream sink2;
    a.assignMission(in2, sink2);
    a.endMission();
    EXPECT(a.isAvailable());
    EXPECT(a.partners().empty());

    // Non-numeric / negative personnel must NOT crash — it re-prompts and
    // accepts the first valid whole number (regression: std::stoi threw).
    CombatSubmarine d("C-020", "Delta");
    std::istringstream in3("Recon\nCmdr Z\nDave\n-3\n7\n");   // reject "Dave" & "-3", accept 7
    std::ostringstream sink3;
    d.assignMission(in3, sink3);
    std::ostringstream out3; d.display(out3);
    EXPECT(out3.str().find("Personnel: 7") != std::string::npos);

    // Blank personnel defaults to 0.
    CombatSubmarine e("C-021", "Echo");
    std::istringstream in4("Recon\nCmdr Q\n\n");
    std::ostringstream sink4;
    e.assignMission(in4, sink4);
    std::ostringstream out4; e.display(out4);
    EXPECT(out4.str().find("Personnel: 0") != std::string::npos);
    REPORT();
}
