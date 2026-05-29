#pragma once

typedef enum riscv_exception {
    RISCV_EXCP_NONE = -1, /* sentinel value */
    RISCV_EXCP_INST_ADDR_MIS = 0x0,
    RISCV_EXCP_INST_ACCESS_FAULT = 0x1,
    RISCV_EXCP_ILLEGAL_INST = 0x2,
    RISCV_EXCP_BREAKPOINT = 0x3,
    RISCV_EXCP_LOAD_ADDR_MIS = 0x4,
    RISCV_EXCP_LOAD_ACCESS_FAULT = 0x5,
    RISCV_EXCP_STORE_AMO_ADDR_MIS = 0x6,
    RISCV_EXCP_STORE_AMO_ACCESS_FAULT = 0x7,
    RISCV_EXCP_U_ECALL = 0x8,
    RISCV_EXCP_S_ECALL = 0x9,
    RISCV_EXCP_VS_ECALL = 0xa,
    RISCV_EXCP_M_ECALL = 0xb,
    RISCV_EXCP_INST_PAGE_FAULT = 0xc, /* since: priv-1.10.0 */
    RISCV_EXCP_LOAD_PAGE_FAULT = 0xd, /* since: priv-1.10.0 */
    RISCV_EXCP_STORE_PAGE_FAULT = 0xf, /* since: priv-1.10.0 */
    RISCV_EXCP_SEMIHOST = 0x10,
    RISCV_EXCP_INST_GUEST_PAGE_FAULT = 0x14,
    RISCV_EXCP_LOAD_GUEST_ACCESS_FAULT = 0x15,
    RISCV_EXCP_VIRT_INSTRUCTION_FAULT = 0x16,
    RISCV_EXCP_STORE_GUEST_AMO_ACCESS_FAULT = 0x17,
} riscv_exception_e;

/* Interrupt causes */
#define IRQ_U_SOFT                         0
#define IRQ_S_SOFT                         1
#define IRQ_VS_SOFT                        2
#define IRQ_M_SOFT                         3
#define IRQ_U_TIMER                        4
#define IRQ_S_TIMER                        5
#define IRQ_VS_TIMER                       6
#define IRQ_M_TIMER                        7
#define IRQ_U_EXT                          8
#define IRQ_S_EXT                          9
#define IRQ_VS_EXT                         10
#define IRQ_M_EXT                          11
#define IRQ_S_GEXT                         12
#define IRQ_PMU_OVF                        13
#define IRQ_LOCAL_MAX                      16
#define IRQ_LOCAL_GUEST_MAX                (32 - 1)

extern void __riscv_vector(void);
void        setup_exception(void *);