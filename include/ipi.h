#pragma once

#include <stdint.h>

/* Host IPI vector IDs (SGI 0-15) */
#define IPI_RESCHEDULE  0
#define IPI_MAX         1

/* Send an IPI to a specific target pCPU.
 * target_cpu: linear CPU ID (not MPIDR). */
void ipi_send_cpu(int target_cpu, uint8_t ipi_vec);

/* Kick target pCPU to re-enter scheduler. */
void ipi_send_reschedule(int target_cpu);

/* Send an IPI to all other pCPUs. */
void ipi_broadcast_others(uint8_t ipi_vec);

/* Handle a pending IPI (called from IRQ handler). */
void ipi_handle(uint8_t ipi_vec);

/* Initialize IPI handling on the current pCPU. */
void ipi_pcpu_init(void);
