#ifndef RECORDS_H
#define RECORDS_H
#include "lnc_types.h"
int record_measurement(uint8_t* buf, uint32_t cap, const measurement_t* m);
int record_event(uint8_t* buf, uint32_t cap, const lnc_event_t* e);
/* shared with keepalive.c: pack timestamp+measurement+mode child TLVs */
int lnc_pack_snapshot(uint8_t* inner, uint32_t cap, uint32_t ts, const measurement_t* m);
#endif
