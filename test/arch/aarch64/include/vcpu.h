#pragma once
#include "vmio.h"
#include "htypes.h"
#include "processor.h"
#include "timer.h"

struct arch_vcpu {
    struct {

        uint64_t x19;
        uint64_t x20;
        uint64_t x21;
        uint64_t x22;
        uint64_t x23;
        uint64_t x24;
        uint64_t x25;
        uint64_t x26;
        uint64_t x27;
        uint64_t x28;

        uint64_t fp; /* x29 */
        uint64_t sp;
        uint64_t pc; /* lr */
    } saved_context;

    void *stack;

    /*
     * Points into ->stack, more convenient than doing pointer arith
     * all the time.
     */
    // struct cpu_info *cpu_info;

    /* Fault Status */

    uint64_t far;
    uint32_t esr;

    uint32_t ifsr; /* 32-bit guests only */
    uint32_t afsr0, afsr1;

    /* MMU */
    uint64_t vbar;
    uint64_t ttbcr;
    uint64_t ttbr0, ttbr1;

    uint32_t dacr; /* 32-bit guests only */
    uint64_t par;

    uint64_t mair;
    uint64_t amair;

    /* Control Registers */
    uint64_t sctlr_el1; /* sctlr_el1 */
    uint64_t actlr;
    uint32_t cpacr;

    uint32_t contextidr;
    uint64_t tpidr_el0;
    uint64_t tpidr_el1;
    uint64_t tpidrro_el0;

    /* HYP configuration */
    uint64_t hcr_el2;
    uint64_t mdcr_el2;

    uint32_t teecr, teehbr; /* ThumbEE, 32-bit guests only */

    /* CP 15 */
    uint32_t csselr;
    uint64_t vmpidr;

    uint64_t lr_mask;


    /* Timer registers  */
    uint64_t cntkctl;

    uint64_t vtimer_ctl;
    vtimer_t phys_timer;
    vtimer_t virt_timer;
#if 0
    struct gic_emul_cpu vgic;
    /* Holds gic context data */
    union gic_state_data gic;

    /* Float-pointer */
    struct vfp_state vfp;
#endif
    bool vtimer_initialized;

    /*
     * The full P2M may require some cleaning (e.g when emulation
     * set/way). As the action can take a long time, it requires
     * preemption. It is deferred until we return to guest, where we can
     * more easily check for softirqs and preempt the vCPU safely.
     */
    bool need_flush_to_ram;
};

typedef struct {
    int vcpu_id;
    int phys_id;
    int priority;

    struct list_head     list;

    atomic_t state;

    struct arch_vcpu     arch;
    struct cpu_user_regs regs;

} vcpu_t;

void    test_vcpu(void);
vcpu_t *create_vcpu(int vcpu_d, int priority);
int     arch_vcpu_init(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack);
void    vcpu_context_save(vcpu_t *vcpu);
void    vcpu_context_restore(vcpu_t *vcpu);
