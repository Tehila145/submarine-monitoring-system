#include "test_util.h"
#include <sstream>
#include "fleet/Menu.h"
#include "fleet/Fleet.h"

using namespace fleet;

int main() {
    // Add a research submarine (1), display all (2), exit (10)
    {
        Fleet f;
        std::istringstream in("1\nresearch\nR-001\nPoseidon\n2\n10\n");
        std::ostringstream out;
        Menu(f, in, out).run();
        EXPECT(out.str().find("R-001") != std::string::npos);
        EXPECT(out.str().find("[Research]") != std::string::npos);
        EXPECT(f.all().size() == 1u);
    }
    // Search a missing serial reports not found
    {
        Fleet f;
        std::istringstream in("3\nX-999\n10\n");
        std::ostringstream out;
        Menu(f, in, out).run();
        EXPECT(out.str().find("not found") != std::string::npos);
    }
    // step() returns false on Exit
    {
        Fleet f;
        std::istringstream in("10\n");
        std::ostringstream out;
        Menu m(f, in, out);
        EXPECT(m.step() == false);
    }
    REPORT();
}
