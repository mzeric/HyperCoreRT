#include "vmmio.h"
#include "htypes.h"
#include "cpu_inline_asm.h"

static u64 read_vmm_physical_ctrl() { return mrs(cnthp_ctl_el2); }

#define TIMER_VMM_VAL_REG     cnthp_tval_el2 // retrun cnthp_tval_el2 = cnthp_cval_el2 - cntpct_el0
#define TIMER_VMM_CMP_VAL_REG cnthp_cval_el2

#define TIMER_VIRTUAL_OFFSET cntoff_el2

#define TIMER_CTRL_ENABLE  (1 << 0)
#define TIMER_CTRL_IT_MASK (1 << 1)
#define TIMER_CTRL_IT_STAT (1 << 2)

/*
    total 30 registers for time

    TimerValue: some view of the physical timer，holds in the register
    Count: the physical ticks
    CompareValue:

two timers provide Count:
    physical timer: COUNTER_TIMER_P_COUNT_REG , cntpct_el0
    virtual timer:  COUNTER_TIMER_V_COUNT_REG , cntvct_el0 = cntpct_el0 - cntvoff_el2

*/
#define COUNTER_TIMER_FRQ_REG cntfrq_el0
/* only highest-EL can write, el1/2/3 read always succeed, el0 may trapped*/

#define COUNTER_TIMER_HYPER_CTL_REG                                                                \
    cnthctl_el2 /* this ctrl not for timer, diffs with cnthp_ctl_el */

#define COUNTER_TIMER_HP_CTL_REG    cnthp_ctl_el2
#define COUNTER_TIMER_HP_CMPVAL_REG cnthp_cval_el2
/*On a write of this register, CNTHP_CVAL_EL2 is set to (CNTPCT_EL0 + TimerValue), where
TimerValue is treated as a signed 32-bit integer.*/
#define COUNTER_TIMER_HP_VAL_REG     cnthp_tval_el2
#define COUNTER_TIMER_HPS_PHY_REG    cnthps_ctl_el2
#define COUNTER_TIMER_HPS_CMPVAL_REG cnthps_cval_el2
#define COUNTER_TIMER_HPS_VAL_REG    cnthps_tval_el2

#define COUNTER_TIMER_HV_CTL_REG     cnthv_ctl_el2
#define COUNTER_TIMER_HV_CMPVAL_REG  cnthv_cval_el2
#define COUNTER_TIMER_HV_VAL_REG     cnthv_tval_el2
#define COUNTER_TIMER_HVS_CTL_REG    cnthvs_ctl_el2
#define COUNTER_TIMER_HVS_CMPVAL_REG cnthvs_cval_el2
#define COUNTER_TIMER_HVS_VAL_REG    cnthvs_tval_el2

#define COUNTER_TIMER_KERNEL_CTL_REG cntkctl_el1

#define COUNTER_TIMER_P_CTL_REG    cntp_ctl_el0
#define COUNTER_TIMER_P_CMPVAL_REG cntp_cval_el0
/*Holds the timer value for the EL1 physical timer.*/
#define COUNTER_TIMER_P_VAL_REG cntp_tval_el0

#define COUNTER_TIMER_SELF_SYNC_P_REG cntpctss_el0
#define COUNTER_TIMER_SELF_SYNC_V_REG cntvctss_el0

/*
        the real timer count:
        offset is applied when read from EL0 or EL1 (if the access is not trapped)
        the offset holds in CNTPOFF_EL2

        complies by regs: CNTHCTL.ECV = 1 and HCR_EL2.{TGE, E2H} = 0



*/

#define COUNTER_TIMER_P_COUNT_REG  cntpct_el0 /*Holds the 64-bit physical count value.*/
#define COUNTER_TIMER_P_OFFSET_REG cntpoff_el2
#define COUNTER_TIMER_V_COUNT_REG  cntvct_el0
#define COUNTER_TIMER_V_OFFSET_REG cntvoff_el2

#define COUNTER_TIMER_PS_CTL_REG    cntps_ctl_el1
#define COUNTER_TIMER_PS_CMPVAL_REG cntps_cval_el1
#define COUNTER_TIMER_PS_VAL_REG    cntps_tval_el1
#define COUNTER_TIMER_V_CTL_REG     cntv_ctl_el0
#define COUNTER_TIMER_V_CMPVAL_REG  cntv_cval_el0
#define COUNTER_TIMER_V_VAL_REG     cntv_tval_el0


/* AArch 64 System Register Encodings */
#define __HSR_SYSREG_c0  0
#define __HSR_SYSREG_c1  1
#define __HSR_SYSREG_c2  2
#define __HSR_SYSREG_c3  3
#define __HSR_SYSREG_c4  4
#define __HSR_SYSREG_c5  5
#define __HSR_SYSREG_c6  6
#define __HSR_SYSREG_c7  7
#define __HSR_SYSREG_c8  8
#define __HSR_SYSREG_c9  9
#define __HSR_SYSREG_c10 10
#define __HSR_SYSREG_c11 11
#define __HSR_SYSREG_c12 12
#define __HSR_SYSREG_c13 13
#define __HSR_SYSREG_c14 14
#define __HSR_SYSREG_c15 15

#define __HSR_SYSREG_0 0
#define __HSR_SYSREG_1 1
#define __HSR_SYSREG_2 2
#define __HSR_SYSREG_3 3
#define __HSR_SYSREG_4 4
#define __HSR_SYSREG_5 5
#define __HSR_SYSREG_6 6
#define __HSR_SYSREG_7 7

/* These are used to decode traps with HSR.EC==HSR_EC_SYSREG */
#define HSR_SYSREG(op0, op1, crn, crm, op2)                                                        \
    (((__HSR_SYSREG_##op0) << HSR_SYSREG_OP0_SHIFT) |                                              \
     ((__HSR_SYSREG_##op1) << HSR_SYSREG_OP1_SHIFT) |                                              \
     ((__HSR_SYSREG_##crn) << HSR_SYSREG_CRN_SHIFT) |                                              \
     ((__HSR_SYSREG_##crm) << HSR_SYSREG_CRM_SHIFT) |                                              \
     ((__HSR_SYSREG_##op2) << HSR_SYSREG_OP2_SHIFT))

#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)

/* Access to system registers */

#define WRITE_SYSREG64(name, v)                                                                    \
    do {                                                                                           \
        uint64_t _r = v;                                                                           \
        asm volatile("msr "__stringify(name) ", %0" : : "r"(_r));                                  \
    } while (0)

#define READ_SYSREG64(name)                                                                        \
    ({                                                                                             \
        uint64_t _r;                                                                               \
        asm volatile("mrs  %0, "__stringify(name) : "=r"(_r));                                     \
        _r;                                                                                        \
    })

void vmm_timer_stop() {
    u64 ctl = mrs(COUNTER_TIMER_HP_CTL_REG);
    ctl |= TIMER_CTRL_IT_MASK;
    ctl &= ~TIMER_CTRL_ENABLE;

    msr(COUNTER_TIMER_HP_CTL_REG, ctl);
}
void  phy_timer_stop() {
    u64 ctl = mrs(cntp_ctl_el0);
    ctl |= TIMER_CTRL_IT_MASK;
    ctl &= ~TIMER_CTRL_ENABLE;

    msr(cntp_ctl_el0, ctl);
}
void vmm_timer_fire(uint64_t val) {
    u64 ctl = mrs(COUNTER_TIMER_HP_CTL_REG);
    ctl |= TIMER_CTRL_ENABLE;
    ctl &= ~TIMER_CTRL_IT_MASK;


    msr(COUNTER_TIMER_HP_VAL_REG, val);
    msr(COUNTER_TIMER_HP_CTL_REG, ctl);
}
void phy_timer_fire(uint64_t val) {
    u64 ctl =0;
    ctl |= TIMER_CTRL_ENABLE;
    ctl &= ~TIMER_CTRL_IT_MASK;


    msr(cntp_tval_el0, val);
    msr(cntp_ctl_el0, ctl);
}

// physical timer
static inline uint64_t read_physical_count() {
    uint64_t val;
    asm volatile("mrs %0, cntpct_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_virtual_count() {
    uint64_t val;
    asm volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

// 读取计数器频率
static inline uint64_t read_cntfrq() {
    uint64_t val;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

// 设置定时器的比较值
static inline void write_cntp_cval(uint64_t val) {
    asm volatile("msr cntp_cval_el0, %0" ::"r"(val));
}

static inline void write_cntp_tval(uint64_t val) {
    asm volatile("msr cntp_tval_el0, %0" ::"r"(val));
}

static inline void enable_timer_interrupt() {
    uint64_t val = 1;
    asm volatile("msr cntp_ctl_el0, %0" ::"r"(val));
}

static inline uint64_t read_offset() {
    uint64_t val;
    // val = mrs(cntpoff_el2);
    // val = mrs(s3_4_c14_c0_6);
    return val;
}

void dealy(uint64_t c) {
    for (int i = 0; i < c; ++i)
        ;
}

void enable_timer_irq(void) { __asm__ __volatile__("msr DAIFClr, %0\n\t" : : "i"(2) : "memory"); }

#define wfi() asm volatile("wfi" ::: "memory")

uint64_t get_cycles() { return mrs(cntpct_el0); }

void timer_init() {

    u64 frq = read_cntfrq();

    enable_timer_irq();

    // vmm_timer_stop();
    // vmm_timer_fire(frq);

    // phy_timer_fire(frq);
    vmm_debug("timer frq: %x(%d)\n", frq, frq);
    // msr(cntp_ctl_el0, 1);
    // write_cntp_tval(frq);

    /* fire hyper physical timer */
    msr(cnthp_ctl_el2, 1);
    msr(cnthp_tval_el2, frq);

    // vmm_info("timer: %x t:%x, c:%x\n", mrs(cntpct_el0), mrs(cntp_tval_el0), mrs(cntp_cval_el0));
    // vmm_info("timer: %x t:%x, c:%x\n", mrs(cntpct_el0), mrs(cntp_tval_el0), mrs(cntp_cval_el0));

}