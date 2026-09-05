#ifndef LOGPLAN_H
#define LOGPLAN_H
#include <stdbool.h>
#define LNC_MAX_LOG_DAYS 7
bool logplan_evict(const char names[][16], int count, const char* today, char out_evict[16]);
#endif
