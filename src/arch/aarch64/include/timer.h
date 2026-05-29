#pragma once

typedef struct {
    uint64_t ctlr; /* control register */
    uint64_t cval; /* compare value */
    uint64_t tval; /* timer value */
}vtimer_t;

uint64_t get_cycles();
void hyp_timer_rearm(void);