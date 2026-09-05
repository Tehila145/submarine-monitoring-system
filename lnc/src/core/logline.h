#ifndef LOGLINE_H
#define LOGLINE_H
#include "lnc_types.h"

/* Text line format shared by the log/events writer (store_log_append /
 * store_events_append) and the range readers (store_log_read_range /
 * store_events_read_range), so a written line always parses back.
 *
 *   measurement: <ts>,<temperature>,<humidity>,<light>,<battery>,<mode>\n
 *   event:       <ts>,<src>,<from>,<to>,<detected>,<after_wd_reset>\n
 *
 * Format functions return the line length (excluding the NUL) or -1 on
 * overflow. Parse functions return true on a well-formed line. */

int  logline_format_measurement(char* buf, int cap, const measurement_t* m);
bool logline_parse_measurement(const char* line, measurement_t* out);
int  logline_format_event(char* buf, int cap, const lnc_event_t* e);
bool logline_parse_event(const char* line, lnc_event_t* out);

#endif
