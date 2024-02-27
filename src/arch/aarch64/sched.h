#pragma once
#include "list.h"

typedef struct hyper_task {
    struct cpu_user_regs regs;
    int id;
    int priority;
    char name[8];
    struct list_head list;
} hyper_task_t;

typedef struct {
	volatile size_t lock;
} spinlock_t;

int init_sched();
int create_task(const char *name, void *entry, int priority);

void sched_yield(struct cpu_user_regs *irq_reg);