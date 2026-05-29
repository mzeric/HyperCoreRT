#pragma once
#include "htypes.h"

int init_timer(void);

uint64_t get_cycles();

int handle_timer_irq(void);