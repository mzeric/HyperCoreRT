#include "vmio.h"
#include "emulate.h"
#include "arch_regs.h"
#include "riscv64_system.h"
#include "inline_asm.h"
#include "exception.h"
#include "htypes.h"
#include "inst_decode.h"
#include "guest_memory.h"
#include "emul_dev.h"
#include "arch_page.h"
#include "mm.h"
#include "safe_printf.h"

/* Read instruction from guest memory at sepc (guest PA) or HTINST after guest MMU is on. */
static u32 vcpu_fetch_inst(struct cpu_user_regs *regs) {
    if (regs->sepc >= PAGE_VIRT_OFFSET) {
        u32 htinst = (u32)csrr(CSR_HTINST);
        if (htinst)
            return htinst;
    }

    uintptr_t host_va = phy_to_vir(regs->sepc);
    uint16_t *p = (uint16_t *)host_va;

    u32 inst = (u32)*p;
    if (!INSN_IS_16BIT(inst)) {
        /* 32-bit instruction: read the upper half */
        inst |= ((u32)*(p + 1)) << 16;
    }
    return inst;
}

/* Decode instruction → byte width (1/2/4/8) for the memory access */
static int inst_to_access_len(u32 inst) {
    /* 16-bit compressed instructions */
    if (INSN_IS_16BIT(inst)) {
        if ((inst & INSN_MASK_C_LD) == INSN_MATCH_C_LD ||
            (inst & INSN_MASK_C_SD) == INSN_MATCH_C_SD ||
            (inst & INSN_MASK_C_LDSP) == INSN_MATCH_C_LDSP ||
            (inst & INSN_MASK_C_SDSP) == INSN_MATCH_C_SDSP)
            return 8; /* 64-bit */
        if ((inst & INSN_MASK_C_LW) == INSN_MATCH_C_LW ||
            (inst & INSN_MASK_C_SW) == INSN_MATCH_C_SW ||
            (inst & INSN_MASK_C_LWSP) == INSN_MATCH_C_LWSP ||
            (inst & INSN_MASK_C_SWSP) == INSN_MATCH_C_SWSP)
            return 4; /* 32-bit */
        return 4; /* default for compressed */
    }

    /* 32-bit instructions */
    if ((inst & INSN_MASK_LD) == INSN_MATCH_LD ||
        (inst & INSN_MASK_SD) == INSN_MATCH_SD)
        return 8;
    if ((inst & INSN_MASK_LW) == INSN_MATCH_LW ||
        (inst & INSN_MASK_SW) == INSN_MATCH_SW)
        return 4;
    if ((inst & INSN_MASK_LH) == INSN_MATCH_LH ||
        (inst & INSN_MASK_SH) == INSN_MATCH_SH)
        return 2;
    if ((inst & INSN_MASK_LB) == INSN_MATCH_LB ||
        (inst & INSN_MASK_SB) == INSN_MATCH_SB)
        return 1;
    if ((inst & INSN_MASK_LBU) == INSN_MATCH_LBU)
        return 1;
    if ((inst & INSN_MASK_LHU) == INSN_MATCH_LHU ||
        (inst & INSN_MASK_LWU) == INSN_MATCH_LWU)
        return 4;

    return 4; /* default */
}

/* Determine if instruction is a store */
static int inst_is_store(u32 inst) {
    if (INSN_IS_16BIT(inst)) {
        if ((inst & INSN_MASK_C_SW) == INSN_MATCH_C_SW ||
            (inst & INSN_MASK_C_SD) == INSN_MATCH_C_SD ||
            (inst & INSN_MASK_C_SWSP) == INSN_MATCH_C_SWSP ||
            (inst & INSN_MASK_C_SDSP) == INSN_MATCH_C_SDSP)
            return 1;
        return 0;
    }

    if ((inst & INSN_MASK_SB) == INSN_MATCH_SB ||
        (inst & INSN_MASK_SH) == INSN_MATCH_SH ||
        (inst & INSN_MASK_SW) == INSN_MATCH_SW ||
        (inst & INSN_MASK_SD) == INSN_MATCH_SD)
        return 1;
    return 0;
}

/* Get the destination register index from the instruction */
static int inst_get_rd(u32 inst) {
    return (inst >> SH_RD) & 0x1F;
}

/*
 * MMIO emulation entry point.
 *
 * Called from do_stage2_fault when a guest page fault hits an emulated device.
 * Decodes the faulting instruction, dispatches to the device driver,
 * and advances the guest PC.
 */
int vcpu_emulate_mmio(vcpu_t *vcpu, struct cpu_user_regs *regs,
                      uint64_t fault_addr, int is_write) {
    u32 inst = vcpu_fetch_inst(regs);
    u64 htinst = csrr(CSR_HTINST);
    u32 decode_inst = inst;

    /*
     * RISC-V H may report a transformed instruction in HTINST. Some
     * transformed stores keep bit0 set but bit1 clear; normalizing bit1
     * restores the standard load/store opcode for register and width decode.
     * Keep the original instruction bits for INSN_LEN() below.
     */
    if (htinst && ((decode_inst & 0x3) == 0x1))
        decode_inst |= 0x2;

    int len = inst_to_access_len(decode_inst);
    int is_store = inst_is_store(decode_inst);
    int access_is_store = is_store || is_write;

    /* Find the device for this address */
    struct mem_region *region = guest_mem_find_region(vcpu, fault_addr, 0);
    if (!region || !region->dev || !region->dev->driver || !region->dev->driver->ops) {
        safe_printf("MMIO: no device for addr=%lx inst=%x\n", fault_addr, inst);
        return -1;
    }

    struct emul_driver_ops *ops = region->dev->driver->ops;

    if (access_is_store) {
        /* Write: extract source register value */
        uint64_t value;
        if (INSN_IS_16BIT(decode_inst)) {
            /* Compressed stores: rs2' is in bits[4:2], maps to x(8+rs2') */
            u32 rs2c = (decode_inst >> 2) & 0x7;
            u32 rs2 = 8 + rs2c; /* x8..x15 */
            value = *((u64 *)regs + rs2);
        } else {
            value = GET_RS2(decode_inst, regs);
        }
        ops->write(region->dev, fault_addr, len, value);
    } else {
        /* Read: call device read and store result into rd (skip x0) */
        uint64_t value = 0;
        ops->read(region->dev, fault_addr, len, &value);

        int rd = inst_get_rd(decode_inst);
        if (rd != 0) {
            SET_RD(decode_inst, regs, value);
        }
    }

    /* Advance PC past the faulting instruction */
    regs->sepc += INSN_LEN(inst);

    return 0;
}

int vcpu_redirect_trap(struct cpu_user_regs *regs, struct cpu_vcpu_trap *trap) {

    u64 vsstatus = csrr(CSR_VSSTATUS);

    /* Change Guest SSTATUS.SPP bit */
    vsstatus &= ~SSTATUS_SPP;
    if (regs->sstatus & SSTATUS_SPP)
        vsstatus |= SSTATUS_SPP;

    /* Change Guest SSTATUS.SPIE bit */
    vsstatus &= ~SSTATUS_SPIE;
    if (regs->sstatus & SSTATUS_SIE)
        vsstatus |= SSTATUS_SPIE;

    /* Clear Guest SSTATUS.SIE bit */
    vsstatus &= ~SSTATUS_SIE;

    vsstatus |= (SSTATUS_SPP | SSTATUS_SPIE | SSTATUS_SIE);

    /* Update Guest SSTATUS */
    csrw(CSR_VSSTATUS, vsstatus);

    /* Update Guest SCAUSE, STVAL, and SEPC */
    csrw(CSR_VSCAUSE, trap->scause);
    csrw(CSR_VSTVAL, trap->stval);
    csrw(CSR_VSEPC, trap->sepc);

    /* Set Guest PC to Guest exception vector */
    regs->sepc = csrr(CSR_VSTVEC);

    return 0;
}

int inject_illegal_inst(struct cpu_user_regs *regs, uint64_t inst) {
    struct cpu_vcpu_trap trap;

    /* Redirect trap to Guest VCPU */
    trap.sepc = regs->sepc;
    trap.scause = RISCV_EXCP_ILLEGAL_INST;
    trap.stval = inst;
    return vcpu_redirect_trap(regs, &trap);
}
