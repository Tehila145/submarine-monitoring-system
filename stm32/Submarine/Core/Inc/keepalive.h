#ifndef KEEPALIVE_H
#define KEEPALIVE_H
#include "lnc_types.h"
int keepalive_build(uint8_t* buf, uint32_t cap, uint32_t ts, const measurement_t* m);
#endif
