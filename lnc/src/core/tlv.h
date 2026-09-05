#ifndef TLV_H
#define TLV_H
#include <stdint.h>
int tlv_write(uint8_t* buf, uint32_t cap, uint8_t tag, const uint8_t* val, uint8_t len);
int tlv_read(const uint8_t* buf, uint32_t len, uint8_t* tag, uint8_t* vlen, const uint8_t** val);
#endif
