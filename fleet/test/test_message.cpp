#include "test_util.h"
#include "fleet/Message.h"

using namespace fleet;

int main() {
    const Submarine* fake = reinterpret_cast<const Submarine*>(0x1234);
    Message m("dive to 200m", fake);
    EXPECT(m.content() == "dive to 200m");
    EXPECT(m.sender() == fake);
    REPORT();
}
