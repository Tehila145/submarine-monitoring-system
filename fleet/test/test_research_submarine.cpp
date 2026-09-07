#include "test_util.h"
#include <sstream>
#include "fleet/ResearchSubmarine.h"

using namespace fleet;

int main() {
    ResearchSubmarine r("R-001", "Poseidon");
    std::istringstream in("Deep-sea vents\nDana Levi, Omer Katz\n");
    std::ostringstream sink;
    r.assignMission(in, sink);
    EXPECT(r.topic() == "Deep-sea vents");
    EXPECT(r.researchers().size() == 2u);
    EXPECT(r.researchers()[0] == "Dana Levi");
    EXPECT(r.researchers()[1] == "Omer Katz");

    std::ostringstream out;
    r.display(out);
    EXPECT(out.str() ==
        "[Research] Serial: R-001 | Name: Poseidon | Status: On mission\n"
        "  Topic: Deep-sea vents\n"
        "  Researchers: Dana Levi, Omer Katz\n");

    r.endMission();
    EXPECT(r.isAvailable());
    EXPECT(r.topic() == "");
    EXPECT(r.researchers().empty());
    REPORT();
}
