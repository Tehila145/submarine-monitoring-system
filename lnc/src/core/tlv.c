#include "tlv.h"
#include <string.h>

int tlv_write(uint8_t* buf, uint32_t cap, uint8_t tag, const uint8_t* val, uint8_t len) {
    uint32_t total = (uint32_t)len + 2u;
    if (cap < total) return -1;
    buf[0] = tag; buf[1] = len;
    if (len) memcpy(&buf[2], val, len);
    return (int)total;
}

int tlv_read(const uint8_t* buf, uint32_t len, uint8_t* tag, uint8_t* vlen, const uint8_t** val) {
    if (len < 2u) return -1;
    uint8_t l = buf[1];
    if ((uint32_t)l + 2u > len) return -1;
    *tag = buf[0]; *vlen = l; *val = &buf[2];
    return (int)l + 2;
}
