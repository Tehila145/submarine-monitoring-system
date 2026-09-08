#include "bytes.h"

void bytes_put_u16(uint8_t* p, uint16_t v) { p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); }
uint16_t bytes_get_u16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1]<<8)); }
void bytes_put_i16(uint8_t* p, int16_t v) { bytes_put_u16(p, (uint16_t)v); }
int16_t bytes_get_i16(const uint8_t* p) { return (int16_t)bytes_get_u16(p); }
void bytes_put_u32(uint8_t* p, uint32_t v) {
    p[0]=(uint8_t)v; p[1]=(uint8_t)(v>>8); p[2]=(uint8_t)(v>>16); p[3]=(uint8_t)(v>>24);
}
uint32_t bytes_get_u32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
