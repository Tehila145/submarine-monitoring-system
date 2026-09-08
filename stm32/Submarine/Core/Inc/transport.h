#ifndef TRANSPORT_H
#define TRANSPORT_H
#include <stdint.h>
/* Transport-independent link to the Central Computer (spec §2.5 NOTE).
 * Exactly one .c implements these; swapping UART<->Ethernet changes only that
 * file. transport_poll is non-blocking: returns 1 with a full TLV frame, else 0. */
int transport_init(void);
int transport_send(const uint8_t* buf, uint32_t len);
int transport_poll(uint8_t* buf, uint32_t cap, uint32_t* out_len);
#endif
