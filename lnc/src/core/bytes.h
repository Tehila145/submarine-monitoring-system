#ifndef BYTES_H
#define BYTES_H
#include <stdint.h>

void     bytes_put_u16(uint8_t* p, uint16_t v);
uint16_t bytes_get_u16(const uint8_t* p);
void     bytes_put_i16(uint8_t* p, int16_t v);
int16_t  bytes_get_i16(const uint8_t* p);
void     bytes_put_u32(uint8_t* p, uint32_t v);
uint32_t bytes_get_u32(const uint8_t* p);

#endif
