#pragma once
#include "htypes.h"

int init_timer(void);

uint64_t get_cycles();

void hyp_timer_rearm(void);

int handle_timer_irq(void);