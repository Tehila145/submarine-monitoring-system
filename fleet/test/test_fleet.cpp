#include "test_util.h"
#include <sstream>
#include <memory>
#include "fleet/Fleet.h"
#include "fleet/ResearchSubmarine.h"
#include "fleet/CombatSubmarine.h"

using namespace fleet;

static CombatSubmarine* addCombat(Fleet& f, const std::string& s, const std::string& n) {
    return static_cast<CombatSubmarine*>(f.add(std::make_unique<CombatSubmarine>(s, n)));
}
static void putOnMission(CombatSubmarine* c) {
    std::istringstream in("Patrol\nCmdr X\n5\n");
    std::ostringstream sink;
    c->assignMission(in, sink);
}

int main() {
    // add rejects duplicate serial
    Fleet f;
    EXPECT(f.add(std::make_unique<ResearchSubmarine>("R-001", "A")) != nullptr);
    EXPECT(f.add(std::make_unique<ResearchSubmarine>("R-001", "B")) == nullptr);
    EXPECT(f.all().size() == 1u);
    EXPECT(f.findBySerial("R-001") != nullptr);
    EXPECT(f.findBySerial("R-999") == nullptr);

    // associate requires both on mission
    Fleet g;
    auto* a = addCombat(g, "C-001", "Nautilus");
    auto* b = addCombat(g, "C-002", "Triton");
    EXPECT(!g.associate(a, b));       // neither on mission
    putOnMission(a); putOnMission(b);
    EXPECT(g.associate(a, b));
    EXPECT(a->sharesMissionWith(b));

    // full-mesh group: c joins a's group -> also linked to b
    auto* c = addCombat(g, "C-003", "Seawolf");
    putOnMission(c);
    EXPECT(g.associate(a, c));
    EXPECT(b->sharesMissionWith(c));
    EXPECT(c->sharesMissionWith(b));

    // sendMessage only within a shared mission
    Fleet h;
    auto* x = addCombat(h, "C-010", "X");
    auto* y = addCombat(h, "C-011", "Y");
    putOnMission(x); putOnMission(y);
    EXPECT(!h.sendMessage(x, y, "hi"));   // not associated yet
    h.associate(x, y);
    EXPECT(h.sendMessage(x, y, "dive"));
    std::ostringstream out;
    y->displayMessages(out);
    EXPECT(out.str() ==
        "Messages for C-011 (1):\n"
        "  from C-010: dive\n");
    REPORT();
}
