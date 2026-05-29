#pragma once

#include "htypes.h"

void uart_register_emul(void);
int uart_vcpu_inject_rx(u8 ch);
void uart_emul_service_host(void);
