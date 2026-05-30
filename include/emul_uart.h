#pragma once

#include "htypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void uart_register_emul(void);
int uart_vcpu_inject_rx(u8 ch);
void uart_emul_service_host(void);

#ifdef __cplusplus
}
#endif