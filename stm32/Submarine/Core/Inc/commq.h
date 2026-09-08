#ifndef COMMQ_H
#define COMMQ_H
#include <stdbool.h>
typedef enum { PRIO_KEEPALIVE=0, PRIO_EVENT=1, PRIO_DATA=2 } tx_prio_t;
typedef struct { bool has_keepalive, has_event, has_data; } commq_state_t;
int commq_next(const commq_state_t* s);
#endif
