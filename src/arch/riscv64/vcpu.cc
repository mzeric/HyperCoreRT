#include "vcpu.h"
#include "kmalloc.h"
#include <string.h>
#include "inline_asm.h"
#include "mmu.h"
#include "exception.h"
#include "guest_memory.h"
#include "emul_dev.h"
#include "riscv_features.h"
#include "guest_dtb.h"

#define VCPU_STACK_SIZE (4096)

static void arch_vcpu_reset(vcpu_t *vcpu) {
    /* nothing yet */
}

vcpu_t *create_vcpu(int vcpu_id, int priority) {
    vcpu_t *vcpu = (vcpu_t *)kmalloc(sizeof(vcpu_t));
    if (!vcpu) {
        return NULL;
    }

    memset(vcpu, 0, sizeof(vcpu_t));
    INIT_LIST_HEAD(&vcpu->list);
    INIT_SPIN_LOCK(vcpu->carch.virt_irq_lock);
    vcpu->vcpu_id = vcpu_id;
    vcpu->priority = priority;

    vcpu->arch.stack = (uint64_t)((char *)kmalloc(VCPU_STACK_SIZE) + VCPU_STACK_SIZE);
    arch_vcpu_reset(vcpu);

    return vcpu;
}

void destroy_vcpu(vcpu_t *vcpu) {
    if (vcpu->arch.stack)
        kfree((void *)(vcpu->arch.stack - VCPU_STACK_SIZE));
    if (vcpu->carch.vregs)
        kfree(vcpu->carch.vregs);
    kfree(vcpu);
}

#define HSTATUS_VS (HSTATUS_SPV | HSTATUS_SPVP)
#define HSTATUS_VCPU_MASK HSTATUS_VS

static bool fpu_dirty(u64 vsstatus) {
    return (vsstatus & SSTATUS_FS) == SSTATUS_FS_DIRTY;
}

static void fpu_save(vcpu_t *vcpu) {
    u64 old_sstatus = csrr(CSR_SSTATUS);
    csrs(sstatus, SSTATUS_FS_DIRTY);

    u64 *f = vcpu->carch.f;
    u64 fcsr;
    asm volatile(
        ".option push\n"
        ".option arch, +d\n"
        "fsd f0, 0(%1)\n"
        "fsd f1, 8(%1)\n"
        "fsd f2, 16(%1)\n"
        "fsd f3, 24(%1)\n"
        "fsd f4, 32(%1)\n"
        "fsd f5, 40(%1)\n"
        "fsd f6, 48(%1)\n"
        "fsd f7, 56(%1)\n"
        "fsd f8, 64(%1)\n"
        "fsd f9, 72(%1)\n"
        "fsd f10, 80(%1)\n"
        "fsd f11, 88(%1)\n"
        "fsd f12, 96(%1)\n"
        "fsd f13, 104(%1)\n"
        "fsd f14, 112(%1)\n"
        "fsd f15, 120(%1)\n"
        "fsd f16, 128(%1)\n"
        "fsd f17, 136(%1)\n"
        "fsd f18, 144(%1)\n"
        "fsd f19, 152(%1)\n"
        "fsd f20, 160(%1)\n"
        "fsd f21, 168(%1)\n"
        "fsd f22, 176(%1)\n"
        "fsd f23, 184(%1)\n"
        "fsd f24, 192(%1)\n"
        "fsd f25, 200(%1)\n"
        "fsd f26, 208(%1)\n"
        "fsd f27, 216(%1)\n"
        "fsd f28, 224(%1)\n"
        "fsd f29, 232(%1)\n"
        "fsd f30, 240(%1)\n"
        "fsd f31, 248(%1)\n"
        "frcsr %0\n"
        ".option pop\n"
        : "=r"(fcsr)
        : "r"(f)
        : "memory");
    vcpu->carch.fcsr = fcsr;

    csrw(CSR_SSTATUS, old_sstatus);
}

static void fpu_restore(vcpu_t *vcpu) {
    u64 old_sstatus = csrr(CSR_SSTATUS);
    csrs(sstatus, SSTATUS_FS_DIRTY);

    u64 *f = vcpu->carch.f;
    u64 fcsr = vcpu->carch.fcsr;
    asm volatile(
        ".option push\n"
        ".option arch, +d\n"
        "fld f0, 0(%0)\n"
        "fld f1, 8(%0)\n"
        "fld f2, 16(%0)\n"
        "fld f3, 24(%0)\n"
        "fld f4, 32(%0)\n"
        "fld f5, 40(%0)\n"
        "fld f6, 48(%0)\n"
        "fld f7, 56(%0)\n"
        "fld f8, 64(%0)\n"
        "fld f9, 72(%0)\n"
        "fld f10, 80(%0)\n"
        "fld f11, 88(%0)\n"
        "fld f12, 96(%0)\n"
        "fld f13, 104(%0)\n"
        "fld f14, 112(%0)\n"
        "fld f15, 120(%0)\n"
        "fld f16, 128(%0)\n"
        "fld f17, 136(%0)\n"
        "fld f18, 144(%0)\n"
        "fld f19, 152(%0)\n"
        "fld f20, 160(%0)\n"
        "fld f21, 168(%0)\n"
        "fld f22, 176(%0)\n"
        "fld f23, 184(%0)\n"
        "fld f24, 192(%0)\n"
        "fld f25, 200(%0)\n"
        "fld f26, 208(%0)\n"
        "fld f27, 216(%0)\n"
        "fld f28, 224(%0)\n"
        "fld f29, 232(%0)\n"
        "fld f30, 240(%0)\n"
        "fld f31, 248(%0)\n"
        "fscsr %1\n"
        ".option pop\n"
        :
        : "r"(f), "r"(fcsr)
        : "memory");

    csrw(CSR_SSTATUS, old_sstatus);
}

static bool vector_dirty(u64 vsstatus) {
    return (vsstatus & SSTATUS_VS) == SSTATUS_VS_DIRTY;
}

static void vector_regs_save(vcpu_t *vcpu) {
    if (!vcpu->carch.vlenb)
        return;

    u64 size = vcpu->carch.vlenb * 32;
    if (!vcpu->carch.vregs) {
        vcpu->carch.vregs = kmalloc(size);
        vcpu->carch.vregs_size = size;
    }
    if (!vcpu->carch.vregs || vcpu->carch.vregs_size < size)
        return;

    void *base = vcpu->carch.vregs;
    u64 vlenb = vcpu->carch.vlenb;
    asm volatile(
        ".option push\n"
        ".option arch, +v\n"
        "mv t0, %0\n"
        "mv t1, %1\n"
        "vsetvli zero, t1, e8, m1, ta, ma\n"
        "vse8.v v0, (t0)\nadd t0, t0, t1\n"
        "vse8.v v1, (t0)\nadd t0, t0, t1\n"
        "vse8.v v2, (t0)\nadd t0, t0, t1\n"
        "vse8.v v3, (t0)\nadd t0, t0, t1\n"
        "vse8.v v4, (t0)\nadd t0, t0, t1\n"
        "vse8.v v5, (t0)\nadd t0, t0, t1\n"
        "vse8.v v6, (t0)\nadd t0, t0, t1\n"
        "vse8.v v7, (t0)\nadd t0, t0, t1\n"
        "vse8.v v8, (t0)\nadd t0, t0, t1\n"
        "vse8.v v9, (t0)\nadd t0, t0, t1\n"
        "vse8.v v10, (t0)\nadd t0, t0, t1\n"
        "vse8.v v11, (t0)\nadd t0, t0, t1\n"
        "vse8.v v12, (t0)\nadd t0, t0, t1\n"
        "vse8.v v13, (t0)\nadd t0, t0, t1\n"
        "vse8.v v14, (t0)\nadd t0, t0, t1\n"
        "vse8.v v15, (t0)\nadd t0, t0, t1\n"
        "vse8.v v16, (t0)\nadd t0, t0, t1\n"
        "vse8.v v17, (t0)\nadd t0, t0, t1\n"
        "vse8.v v18, (t0)\nadd t0, t0, t1\n"
        "vse8.v v19, (t0)\nadd t0, t0, t1\n"
        "vse8.v v20, (t0)\nadd t0, t0, t1\n"
        "vse8.v v21, (t0)\nadd t0, t0, t1\n"
        "vse8.v v22, (t0)\nadd t0, t0, t1\n"
        "vse8.v v23, (t0)\nadd t0, t0, t1\n"
        "vse8.v v24, (t0)\nadd t0, t0, t1\n"
        "vse8.v v25, (t0)\nadd t0, t0, t1\n"
        "vse8.v v26, (t0)\nadd t0, t0, t1\n"
        "vse8.v v27, (t0)\nadd t0, t0, t1\n"
        "vse8.v v28, (t0)\nadd t0, t0, t1\n"
        "vse8.v v29, (t0)\nadd t0, t0, t1\n"
        "vse8.v v30, (t0)\nadd t0, t0, t1\n"
        "vse8.v v31, (t0)\n"
        ".option pop\n"
        :
        : "r"(base), "r"(vlenb)
        : "t0", "t1", "memory");
}

static void vector_regs_restore(vcpu_t *vcpu) {
    if (!vcpu->carch.vregs || !vcpu->carch.vlenb)
        return;

    void *base = vcpu->carch.vregs;
    u64 vlenb = vcpu->carch.vlenb;
    asm volatile(
        ".option push\n"
        ".option arch, +v\n"
        "mv t0, %0\n"
        "mv t1, %1\n"
        "vsetvli zero, t1, e8, m1, ta, ma\n"
        "vle8.v v0, (t0)\nadd t0, t0, t1\n"
        "vle8.v v1, (t0)\nadd t0, t0, t1\n"
        "vle8.v v2, (t0)\nadd t0, t0, t1\n"
        "vle8.v v3, (t0)\nadd t0, t0, t1\n"
        "vle8.v v4, (t0)\nadd t0, t0, t1\n"
        "vle8.v v5, (t0)\nadd t0, t0, t1\n"
        "vle8.v v6, (t0)\nadd t0, t0, t1\n"
        "vle8.v v7, (t0)\nadd t0, t0, t1\n"
        "vle8.v v8, (t0)\nadd t0, t0, t1\n"
        "vle8.v v9, (t0)\nadd t0, t0, t1\n"
        "vle8.v v10, (t0)\nadd t0, t0, t1\n"
        "vle8.v v11, (t0)\nadd t0, t0, t1\n"
        "vle8.v v12, (t0)\nadd t0, t0, t1\n"
        "vle8.v v13, (t0)\nadd t0, t0, t1\n"
        "vle8.v v14, (t0)\nadd t0, t0, t1\n"
        "vle8.v v15, (t0)\nadd t0, t0, t1\n"
        "vle8.v v16, (t0)\nadd t0, t0, t1\n"
        "vle8.v v17, (t0)\nadd t0, t0, t1\n"
        "vle8.v v18, (t0)\nadd t0, t0, t1\n"
        "vle8.v v19, (t0)\nadd t0, t0, t1\n"
        "vle8.v v20, (t0)\nadd t0, t0, t1\n"
        "vle8.v v21, (t0)\nadd t0, t0, t1\n"
        "vle8.v v22, (t0)\nadd t0, t0, t1\n"
        "vle8.v v23, (t0)\nadd t0, t0, t1\n"
        "vle8.v v24, (t0)\nadd t0, t0, t1\n"
        "vle8.v v25, (t0)\nadd t0, t0, t1\n"
        "vle8.v v26, (t0)\nadd t0, t0, t1\n"
        "vle8.v v27, (t0)\nadd t0, t0, t1\n"
        "vle8.v v28, (t0)\nadd t0, t0, t1\n"
        "vle8.v v29, (t0)\nadd t0, t0, t1\n"
        "vle8.v v30, (t0)\nadd t0, t0, t1\n"
        "vle8.v v31, (t0)\n"
        ".option pop\n"
        :
        : "r"(base), "r"(vlenb)
        : "t0", "t1", "memory");
}

static void vector_csr_save(vcpu_t *vcpu) {
    u64 old_sstatus = csrr(CSR_SSTATUS);
    csrs(sstatus, SSTATUS_VS_DIRTY);

    vcpu->carch.vstart = csrr(CSR_VSTART);
    vcpu->carch.vxsat  = csrr(CSR_VXSAT);
    vcpu->carch.vxrm   = csrr(CSR_VXRM);
    vcpu->carch.vcsr   = csrr(CSR_VCSR);
    vcpu->carch.vl     = csrr(CSR_VL);
    vcpu->carch.vtype  = csrr(CSR_VTYPE);
    vcpu->carch.vlenb  = csrr(CSR_VLENB);
    vector_regs_save(vcpu);

    csrw(CSR_SSTATUS, old_sstatus);
}

static void vector_csr_restore(vcpu_t *vcpu) {
    u64 old_sstatus = csrr(CSR_SSTATUS);
    csrs(sstatus, SSTATUS_VS_DIRTY);

    vector_regs_restore(vcpu);
    csrw(CSR_VSTART, vcpu->carch.vstart);
    csrw(CSR_VXSAT,  vcpu->carch.vxsat);
    csrw(CSR_VXRM,   vcpu->carch.vxrm);
    csrw(CSR_VCSR,   vcpu->carch.vcsr);
    asm volatile(
        ".option push\n"
        ".option arch, +v\n"
        "vsetvl zero, %0, %1\n"
        ".option pop\n"
        :
        : "r"(vcpu->carch.vl), "r"(vcpu->carch.vtype)
        : "memory");

    csrw(CSR_SSTATUS, old_sstatus);
}

int arch_vcpu_init(vcpu_t *vcpu, uintptr_t entry, uintptr_t stack) {
    vcpu->arch.stack = stack;

    /* Set up guest entry in the trap frame */
    vcpu->regs.sepc = entry;
    vcpu->regs.ra = entry;
    vcpu->regs.sp = stack;
    vcpu->regs.a0 = 0;
    vcpu->regs.a1 = riscv_guest_dtb_addr();

    /* Set HSTATUS to enter VS-mode on sret */
    vcpu->carch.hstatus = HSTATUS_VS;

    /* Set VSSTATUS: SPP=0 (return to U-mode within guest), SPIE=1 */
    vcpu->carch.vsstatus = SSTATUS_SPIE;
    if (riscv_has_fpu())
        vcpu->carch.vsstatus |= SSTATUS_FS_INITIAL;
    if (riscv_has_vector())
        vcpu->carch.vsstatus |= SSTATUS_VS_INITIAL;

    /* Set up guest memory regions */
    INIT_LIST_HEAD(&vcpu->mem_region);

    /* Guest RAM: identity-mapped, RWX */
    struct mem_region *ram = (struct mem_region *)kmalloc(sizeof(struct mem_region));
    memset(ram, 0, sizeof(*ram));
    ram->gpa  = riscv_guest_ram_base();
    ram->hpa  = riscv_guest_ram_base();
    ram->size = riscv_guest_ram_size();
    ram->attr = MEM_ACCESS_RWX | PAGE_ATTR_USER;
    ram->dev  = NULL;
    ram->match_name[0] = '\0';
    guest_mem_add_region(vcpu, ram);

    /* Guest UART: 0x20000000, emulated NS16550A, no stage-2 map */
    struct mem_region *uart = (struct mem_region *)kmalloc(sizeof(struct mem_region));
    memset(uart, 0, sizeof(*uart));
    uart->gpa  = 0x20000000;
    uart->hpa  = 0x0;
    uart->size = 0x1000; /* 4KB */
    uart->attr = MEM_ACCESS_NONE;
    uart->dev  = NULL;
    strncpy(uart->match_name, "ns16550a", sizeof(uart->match_name) - 1);
    probe_emul_dev(uart);
    guest_mem_add_region(vcpu, uart);

    /* Guest PLIC: emulated so virtual UART IRQs can be injected via VSEIP. */
    struct mem_region *plic = (struct mem_region *)kmalloc(sizeof(struct mem_region));
    memset(plic, 0, sizeof(*plic));
    plic->gpa  = 0x0C000000;
    plic->hpa  = 0x0;
    plic->size = 0x400000;
    plic->attr = MEM_ACCESS_NONE;
    plic->dev  = NULL;
    strncpy(plic->match_name, "riscv-plic", sizeof(plic->match_name) - 1);
    probe_emul_dev(plic);
    guest_mem_add_region(vcpu, plic);

    return 0;
}

void vcpu_context_save(vcpu_t *vcpu) {
    /* Save H-extension CSRs */
    vcpu->carch.hstatus   = csrr(CSR_HSTATUS) & HSTATUS_VCPU_MASK;
    vcpu->carch.vsstatus  = csrr(CSR_VSSTATUS);
    vcpu->carch.vsie      = csrr(CSR_VSIE);
    vcpu->carch.vsepc     = csrr(CSR_VSEPC);
    vcpu->carch.vstvec    = csrr(CSR_VSTVEC);
    vcpu->carch.vsscratch = csrr(CSR_VSSCRATCH);
    vcpu->carch.vscause   = csrr(CSR_VSCAUSE);
    vcpu->carch.vstval    = csrr(CSR_VSTVAL);
    vcpu->carch.vsatp     = csrr(CSR_VSATP);
    vcpu->carch.hie       = csrr(CSR_HIE);

    /* HVIP is a hardware projection of virt_irq_pending, not saved vCPU state. */
    csrw(CSR_HVIP, 0);

    if (riscv_has_fpu() && fpu_dirty(vcpu->carch.vsstatus))
        fpu_save(vcpu);
    if (riscv_has_vector() && vector_dirty(vcpu->carch.vsstatus))
        vector_csr_save(vcpu);
}

void vcpu_context_restore(vcpu_t *vcpu) {
    if (riscv_has_fpu() && fpu_dirty(vcpu->carch.vsstatus))
        fpu_restore(vcpu);
    if (riscv_has_vector() && vector_dirty(vcpu->carch.vsstatus))
        vector_csr_restore(vcpu);

    /* Restore VS-mode CSRs */
    csrw(CSR_VSSTATUS,  vcpu->carch.vsstatus);
    csrw(CSR_VSIE,      vcpu->carch.vsie);
    csrw(CSR_VSEPC,     vcpu->carch.vsepc);
    csrw(CSR_VSTVEC,    vcpu->carch.vstvec);
    csrw(CSR_VSSCRATCH, vcpu->carch.vsscratch);
    csrw(CSR_VSCAUSE,   vcpu->carch.vscause);
    csrw(CSR_VSTVAL,    vcpu->carch.vstval);
    csrw(CSR_VSATP,     vcpu->carch.vsatp);

    /* Restore hypervisor interrupt configuration */
    csrw(CSR_HIE,  vcpu->carch.hie);
    csrw(CSR_HVIP, 0);

    /* Delegate guest-local traps back to VS-mode. Keep VS ecalls in HS for SBI emulation. */
    csrw(CSR_HEDELEG, (1UL << RISCV_EXCP_INST_ADDR_MIS) |
                       (1UL << RISCV_EXCP_ILLEGAL_INST) |
                       (1UL << RISCV_EXCP_BREAKPOINT) |
                       (1UL << RISCV_EXCP_LOAD_ADDR_MIS) |
                       (1UL << RISCV_EXCP_STORE_AMO_ADDR_MIS) |
                       (1UL << RISCV_EXCP_U_ECALL) |
                       (1UL << RISCV_EXCP_INST_PAGE_FAULT) |
                       (1UL << RISCV_EXCP_LOAD_PAGE_FAULT) |
                       (1UL << RISCV_EXCP_STORE_PAGE_FAULT));

    /* Delegate interrupts to VS-mode: VS-mode timer, external, software */
    csrw(CSR_HIDELEG, (1UL << IRQ_VS_SOFT) |
                      (1UL << IRQ_VS_TIMER) |
                      (1UL << IRQ_VS_EXT));

    /* Restore only vCPU-owned HSTATUS return bits; keep host policy bits intact. */
    u64 hstatus = csrr(CSR_HSTATUS);
    hstatus &= ~HSTATUS_VCPU_MASK;
    hstatus |= vcpu->carch.hstatus & HSTATUS_VCPU_MASK;
    csrw(CSR_HSTATUS, hstatus);
}
