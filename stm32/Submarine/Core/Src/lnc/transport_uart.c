#include "transport.h"
#include "board.h"   /* huart2, HAL */
#include <string.h>

int transport_init(void) { return 0; }  /* USART2 already inited by CubeMX */

int transport_send(const uint8_t* buf, uint32_t len) {
    return (HAL_UART_Transmit(BOARD_CENTRAL_UART, (uint8_t*)buf, (uint16_t)len, HAL_MAX_DELAY)
            == HAL_OK) ? 0 : -1;
}

/* Non-blocking RX: pull available bytes (timeout 0) and assemble one TLV frame
 * [tag][len][len bytes]. Returns 1 with a complete frame in buf, else 0. */
int transport_poll(uint8_t* buf, uint32_t cap, uint32_t* out_len) {
    static uint8_t frame[64];
    static uint32_t have = 0, need = 0;
    uint8_t b;
    while (HAL_UART_Receive(BOARD_CENTRAL_UART, &b, 1, 0) == HAL_OK) {
        if (have < sizeof(frame)) frame[have++] = b;
        if (have == 2) need = 2u + frame[1];
        if (have >= 2 && have == need) {
            uint32_t n = need;
            have = 0; need = 0;
            if (n <= cap) { memcpy(buf, frame, n); *out_len = n; return 1; }
            return 0;   /* frame too big for caller buffer; drop it */
        }
    }
    return 0;
}
