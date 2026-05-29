#include "sched.h"
#include "sched_simple.h"
#include "spin_lock.h"
#include "smp.h"
#include "stdbool.h"
#include "vmio.h"

struct list_head     g_ready_list;
struct list_head     g_wait_list;
static spinlock_t    g_sched_lock = { .lock = SPIN_UNLOCKED };

bool is_task_in_chain(hyper_task_t *task) {
    struct list_head *entry = &task->list;
    if(entry->next == NULL || entry->prev == NULL)
        return false;
    return (entry->next->prev == entry && entry->prev->next == entry);
}

void __chain_extract_task(hyper_task_t *task) {
    BUG_ON(!task);
    if(is_task_in_chain(task))
        list_del(&task->list);

    task->list.next = NULL;
    task->list.prev = NULL;
}

static void __insert_task_sorted(struct list_head *head, hyper_task_t *new_task) {
    hyper_task_t     *pos;
    struct list_head *p = head;

    list_for_each_entry(pos, head, list) {
        if (new_task->priority < pos->priority) {
            p = &pos->list;
            break;
        }
    }

    list_add_tail(&(new_task->list), p);
}

/* ---- Locked public API ---- */

hyper_task_t *simple_scheduler_next(void)
{
    int my_cpu = cpu_id();

    arch_spin_lock(&g_sched_lock);

    /* Find the first task matching our affinity */
    hyper_task_t *next = NULL, *pos;
    list_for_each_entry(pos, &g_ready_list, list) {
        if (pos->pcpu_affinity == -1 || pos->pcpu_affinity == my_cpu) {
            next = pos;
            break;
        }
    }

    if (!next) {
        arch_spin_unlock(&g_sched_lock);
        return NULL;
    }

    hyper_task_t *current = current_task();
    if (current && current->priority < next->priority) {
        arch_spin_unlock(&g_sched_lock);
        return NULL;
    }

    __chain_extract_task(next);
    set_current(next);

    arch_spin_unlock(&g_sched_lock);
    return next;
}

void simple_scheduler_sched(hyper_task_t *task)
{
    arch_spin_lock(&g_sched_lock);
    __insert_task_sorted(&g_ready_list, task);
    arch_spin_unlock(&g_sched_lock);
}

void simple_scheduler_yield(hyper_task_t *task)
{
    arch_spin_lock(&g_sched_lock);
    __chain_extract_task(task);
    __insert_task_sorted(&g_ready_list, task);
    arch_spin_unlock(&g_sched_lock);
}

void simple_scheduler_block(hyper_task_t *task)
{
    arch_spin_lock(&g_sched_lock);
    __chain_extract_task(task);
    __insert_task_sorted(&g_wait_list, task);
    arch_spin_unlock(&g_sched_lock);
}

void simple_scheduler_unblock(hyper_task_t *task)
{
    arch_spin_lock(&g_sched_lock);
    __chain_extract_task(task);
    __insert_task_sorted(&g_ready_list, task);
    arch_spin_unlock(&g_sched_lock);
}

int simple_scheduler_init(void)
{
    INIT_LIST_HEAD(&g_ready_list);
    INIT_LIST_HEAD(&g_wait_list);

    return 0;
}
