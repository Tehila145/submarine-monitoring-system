#include "test_util.h"
#include "objectdet_logic.h"

int main(void) {
    objectdet_logic_reset();
    lnc_event_t e;
    EXPECT(objectdet_logic_step(false, 1, &e) == false);  /* no change */
    EXPECT(objectdet_logic_step(true, 2, &e) == true);    /* detected */
    EXPECT(e.src == SRC_OBJECT && e.data.object.detected == true);
    EXPECT(objectdet_logic_step(true, 3, &e) == false);   /* still present */
    EXPECT(objectdet_logic_step(false, 4, &e) == true);   /* cleared */
    EXPECT(e.data.object.detected == false);
    REPORT();
}
