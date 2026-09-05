#include "logplan.h"
#include <string.h>

bool logplan_evict(const char names[][16], int count, const char* today, char out_evict[16]) {
    for (int i = 0; i < count; ++i)
        if (strcmp(names[i], today) == 0) return false;   /* today already open */
    if (count < LNC_MAX_LOG_DAYS) return false;
    strncpy(out_evict, names[0], 16);                     /* sorted asc -> oldest */
    out_evict[15] = '\0';
    return true;
}
