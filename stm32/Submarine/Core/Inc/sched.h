#ifndef SCHED_H
#define SCHED_H
#include <stdint.h>
#include <stdbool.h>
/* Non-blocking periodic gate. Returns true and advances *last_ms when
 * (now_ms - *last_ms) >= period_ms. Uses uint32_t wrap-around arithmetic,
 * so it is correct across the millisecond counter rolling over. */
bool sched_due(uint32_t* last_ms, uint32_t period_ms, uint32_t now_ms);
#endif
