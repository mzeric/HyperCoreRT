/*
 * RISC-V IPI stubs — Phase 1 single-hart, no IPI needed yet.
 */
#include "ipi.h"

void ipi_send_reschedule(int target_cpu) {
    /* No-op for single-hart */
}
