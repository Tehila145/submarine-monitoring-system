#include "test_util.h"
#include "commq.h"

int main(void) {
    commq_state_t s = { true, true, true };
    EXPECT(commq_next(&s) == PRIO_KEEPALIVE);
    s = (commq_state_t){ false, true, true };
    EXPECT(commq_next(&s) == PRIO_EVENT);
    s = (commq_state_t){ false, false, true };
    EXPECT(commq_next(&s) == PRIO_DATA);
    s = (commq_state_t){ false, false, false };
    EXPECT(commq_next(&s) == -1);
    REPORT();
}
