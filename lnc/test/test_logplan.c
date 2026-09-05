#include "test_util.h"
#include "logplan.h"
#include <string.h>

int main(void) {
    char names[8][16]; char evict[16];
    /* today already present -> no eviction */
    strcpy(names[0], "2026-09-01"); strcpy(names[1], "2026-09-04");
    EXPECT(logplan_evict(names, 2, "2026-09-04", evict) == false);

    /* 7 existing, new day -> evict oldest (index 0) */
    const char* d[7] = {"2026-08-28","2026-08-29","2026-08-30","2026-08-31",
                        "2026-09-01","2026-09-02","2026-09-03"};
    for (int i=0;i<7;i++) strcpy(names[i], d[i]);
    EXPECT(logplan_evict(names, 7, "2026-09-04", evict) == true);
    EXPECT(strcmp(evict, "2026-08-28") == 0);
    REPORT();
}
