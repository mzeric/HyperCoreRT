/*
 * AArch64 system-register access helpers and ESR decoding constants.
 *
 * Two layers live here:
 *
 *   - mrs_s / msr_s: C wrappers around the encoded MRS_S / MSR_S
 *     instructions, so that registers without a friendly mnemonic
 *     can be read/written by symbolic op0/op1/CRn/CRm/op2 tuple.
 *     The accompanying assembler macros are emitted into the toolchain
 *     scope via the inline asm block (or via the .S helper path when
 *     compiled with __ASSEMBLY__).
 *
 *   - HSR_SYSREG_*: bit positions inside ESR_ELx.ISS for a trapped
 *     MRS/MSR (EC == 0x18), used by the guest sysreg trap handler
 *     to reconstruct the (op0,op1,CRn,CRm,op2,Rt) of the trapped
 *     instruction.
 *
 * The (op0,op1,CRn,CRm,op2) -> 21-bit encoding itself lives in
 * include/compiler.h (sys_reg(), sys_reg_Op0(), ...) so it can be
 * reused by code that needs the packed form without pulling in the
 * AArch64-only assembly trampolines below.
 */

#pragma once

#include "compiler.h"


/* ----------------------------------------------------------------- *
 * MRS_S / MSR_S trampolines.                                        *
 *                                                                   *
 * The AArch64 ISA carves out two encodings whose immediate operand  *
 * is the 21-bit sys_reg(...) tuple; these forms let us reach any    *
 * AArch64 system register (including ones the assembler does not    *
 * yet know about) without resorting to .inst by hand.               *
 *                                                                   *
 * We materialise the .macro definitions once, either into the .S    *
 * source directly when __ASSEMBLY__ is set, or into the toolchain's *
 * inline-asm scope so the C wrappers further down can refer to them.*
 * ----------------------------------------------------------------- */

#ifdef __ASSEMBLY__

        .irp    num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15, \
                    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30
        .equ    .L__reg_num_x\num, \num
        .endr
        .equ    .L__reg_num_xzr, 31

        .macro  mrs_s, rt, sreg
        .inst   (0xd5200000 | (\sreg) | (.L__reg_num_\rt))
        .endm

        .macro  msr_s, sreg, rt
        .inst   (0xd5000000 | (\sreg) | (.L__reg_num_\rt))
        .endm

#else /* !__ASSEMBLY__ */

asm(
"       .irp    num,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30\n"
"       .equ    .L__reg_num_x\\num, \\num\n"
"       .endr\n"
"       .equ    .L__reg_num_xzr, 31\n"
"\n"
"       .macro  mrs_s, rt, sreg\n"
"       .inst   0xd5200000|(\\sreg)|(.L__reg_num_\\rt)\n"
"       .endm\n"
"\n"
"       .macro  msr_s, sreg, rt\n"
"       .inst   0xd5000000|(\\sreg)|(.L__reg_num_\\rt)\n"
"       .endm\n"
);

/* Read a register identified by its sys_reg(...) encoding. */
#define mrs_s(_enc)                                                 \
        ({                                                          \
            u64 _v;                                                 \
            asm volatile ("mrs_s %0, " stringify(_enc)              \
                          : "=r"(_v));                              \
            _v;                                                     \
        })

/* Write a register identified by its sys_reg(...) encoding. */
#define msr_s(_enc, _v)                                             \
        do {                                                        \
            asm volatile ("msr_s " stringify(_enc) ", %0"           \
                          :                                         \
                          : "r"(_v));                               \
        } while (0)

#endif /* __ASSEMBLY__ */


/* sys_insn is just sys_reg() repurposed for the AArch64 system
 * instruction class (TLBI / DC / IC / AT); the layout is identical. */
#define sys_insn sys_reg


/* ----------------------------------------------------------------- *
 * ESR_ELx.ISS layout for trapped MRS/MSR (EC == 0x18).              *
 *                                                                   *
 * The 32-bit ISS scatters (op0, op1, CRn, CRm, op2, Rt) into the    *
 * following fields:                                                 *
 *                                                                   *
 *   bits  [21:20]  Op0                                              *
 *   bits  [19:17]  Op2                                              *
 *   bits  [16:14]  Op1                                              *
 *   bits  [13:10]  CRn                                              *
 *   bits  [ 9: 5]  Rt (handled elsewhere)                           *
 *   bits  [ 4: 1]  CRm                                              *
 *   bit   [    0]  Direction (Read/Write)                           *
 *                                                                   *
 * The handler unpacks these, ORs them back through sys_reg(...) and *
 * dispatches on the resulting encoding.                             *
 * ----------------------------------------------------------------- */

#define HSR_SYSREG_OP0_MASK   0x00300000u
#define HSR_SYSREG_OP0_SHIFT  20

#define HSR_SYSREG_OP1_MASK   0x0001c000u
#define HSR_SYSREG_OP1_SHIFT  14

#define HSR_SYSREG_CRN_MASK   0x00003c00u
#define HSR_SYSREG_CRN_SHIFT  10

#define HSR_SYSREG_CRM_MASK   0x0000001eu
#define HSR_SYSREG_CRM_SHIFT  1

#define HSR_SYSREG_OP2_MASK   0x000e0000u
#define HSR_SYSREG_OP2_SHIFT  17

#define HSR_SYSREG_REGS_MASK  ( HSR_SYSREG_OP0_MASK | HSR_SYSREG_OP1_MASK | \
                                HSR_SYSREG_CRN_MASK | HSR_SYSREG_CRM_MASK | \
                                HSR_SYSREG_OP2_MASK )
