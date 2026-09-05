#include "test_util.h"
#include "sched.h"

int main(void) {
    uint32_t last = 0;
    EXPECT(sched_due(&last, 5000, 4999) == false);   /* not yet */
    EXPECT(sched_due(&last, 5000, 5000) == true);     /* due; last -> 5000 */
    EXPECT(last == 5000u);
    EXPECT(sched_due(&last, 5000, 9999) == false);    /* still within period */
    EXPECT(sched_due(&last, 5000, 10000) == true);    /* due again */

    /* wrap-around across the uint32 rollover:
       last = 0xFFFFFF00, now = 0x00000050 -> elapsed = 0x150 (336) */
    last = 0xFFFFFF00u;
    EXPECT(sched_due(&last, 256, 0x00000050u) == true);   /* 336 >= 256 */
    EXPECT(last == 0x00000050u);
    EXPECT(sched_due(&last, 256, 0x00000060u) == false);  /* elapsed 16 < 256 */
    REPORT();
}
