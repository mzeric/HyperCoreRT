#pragma once
#include "config.h"
#include "list.h"
#include "arch_regs.h"
#include "spin_lock.h"
#include "vcpu.h"


#define VM_VCPU_TASK_STACK_SIZE 4096
#define VM_VCPU_TASK_F_ARG0_VALID (1ull << 0)

struct vm_vcpu_task_desc {
    const char *name;
    int         vcpu_id;
    int         priority;
    int         vcpu_priority;
    int         pcpu_affinity;
    uintptr_t   entry;
    uintptr_t   arg0;
    uintptr_t   arg1;
    uint64_t    guest_cpu_id;
    uint64_t    flags;
};


enum task_state {
    TASK_RUNNING = 1,
    TASK_PAUSE,
    TASK_READY,
    TASK_EXIT,
};

#define VCPU_MAX_PENDING_VIRQ 8
typedef struct hyper_task {
    struct cpu_user_regs regs;
    int                  id;
    int                  priority;
    char                 name[8];
    struct list_head     list;
    enum task_state      state;
    vcpu_t              *vcpu;

    /* Simple pending vIRQ ring for this vcpu (PPIs + SGIs).
       Flushed into ICH_LRn_EL2 by __el2_switch_to on context restore. */
    int                  pending_virq[VCPU_MAX_PENDING_VIRQ];
    int                  pending_virq_count;
    spinlock_t           virq_lock;

    /* Linux MPIDR affinity bits as advertised in dts cpu@N/reg */
    uint64_t             mpidr;

    /* Host pCPU affinity: -1 = any, 0..N = pinned to pCPU N */
    int                  pcpu_affinity;

    /* Debug counters per vCPU */
    uint64_t             trap_count;
    uint64_t             irq_count;
    uint64_t             switch_count;
} hyper_task_t;



int init_sched();
int create_task(const char *name, void *entry, int priority);
int create_task2(const char *name, void *entry, int priority);
int create_task3(const char *name, void *__entry, int priority);
int vm_create_vcpu_task(const struct vm_vcpu_task_desc *desc);

#if defined(__riscv)
int riscv_create_guest_vcpu(u64 hartid, u64 entry, u64 a1, int priority);
hyper_task_t *riscv_find_guest_vcpu(u64 hartid);
void riscv_mark_boot_vcpu_started(void);
int riscv_boot_vcpu_started(void);
#endif

void sched_yield(struct cpu_user_regs *irq_reg);
hyper_task_t *current_task(void);
void set_current(void *c);
void sink_task(void);

/* Global running-task snapshot indexed by pCPU id. */
extern hyper_task_t *g_running[CONFIG_SMP_CPU_NUM];

void sched_yield2(struct cpu_user_regs *irq_reg);
