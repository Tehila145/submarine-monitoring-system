#include "test_util.h"
#include "fleet/text.h"

using namespace fleet;

int main() {
    // trim
    EXPECT(trim("  hi  ") == "hi");
    EXPECT(trim("nospace") == "nospace");
    EXPECT(trim("   ") == "");

    // split: trims each piece, drops empties
    std::vector<std::string> expected{"Dana Levi", "Omer Katz"};
    EXPECT(split(" Dana Levi , Omer Katz ,", ',') == expected);

    // join
    EXPECT(join({"a", "b", "c"}, ", ") == "a, b, c");
    EXPECT(join({}, ", ") == "");
    REPORT();
}
