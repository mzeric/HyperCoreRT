#pragma once

#include <stdint.h>

int riscv_ipi_send_cpu_sync(int target_cpu, uint8_t ipi_vec);
uint32_t riscv_ipi_take_pending(int cpu);
