#include "htypes.h"
#include "mmu.h"
#include "compiler.h"
#include "aarch64_system.h"

int get_pa_bits() {
    uint8_t pa_ps = mrs(ID_AA64MMFR0_EL1) & 0xFu;
    return pa_ps;
}
