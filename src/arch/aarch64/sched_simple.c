#include "sched.h"
#include "sched_simple.h"
#include "vmmlib.h"
#include "vmmio.h"

struct list_head     g_ready_list;
struct list_head     g_wait_list;
extern hyper_task_t *g_current_task;

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

hyper_task_t *get_ready_task() {
    return list_first_entry_or_null(&g_ready_list, hyper_task_t, list);
}

hyper_task_t *pick_ready_task() {
    hyper_task_t *task = get_ready_task();
    if (!task) {
        vmm_info("no task available\n");
        return NULL;
    }

    __chain_extract_task(task);
    g_current_task = task;

    return task;
}

hyper_task_t *simple_scheduler_next() {
    hyper_task_t *next = list_first_entry_or_null(&g_ready_list, hyper_task_t, list);

    if(!next)
        return next;

    hyper_task_t *current = current_task();
    if (current && current->priority < next->priority) {
        vmm_info("current task-%d is high then task-%d\n", current->id, next->id);
        return NULL;
    }
    next = pick_ready_task();
    return next;
}

void __insert_task_sorted(struct list_head *head, hyper_task_t *new_task) {
    hyper_task_t     *pos;
    struct list_head *p = head;

    // 遍历链表，找到合适的插入位置
    list_for_each_entry(pos, head, list) {
        if (new_task->priority < pos->priority) {
            // 找到了插入位置
            p = &pos->list;
            break;
        }
    }

    // 插入到找到的位置之前（如果找到了比新任务priority大的任务），
    // 或者插入到具有相同priority的最后一个任务之后
    list_add_tail(&(new_task->list), p);
}



void simple_scheduler_insert(hyper_task_t *task, struct list_head *head) {
    __insert_task_sorted(head, task);
}

void simple_scheduler_yield(hyper_task_t *task) {
    __chain_extract_task(task);
    simple_scheduler_insert(task, &g_ready_list);
}
void simple_scheduler_sched(hyper_task_t *task) {
    simple_scheduler_insert(task, &g_ready_list);
}

void simple_scheduler_block(hyper_task_t *task) {
    __chain_extract_task(task);
    simple_scheduler_insert(task, &g_wait_list);
}

void simple_scheduler_unblock(hyper_task_t *task) {
    __chain_extract_task(task);
    simple_scheduler_insert(task, &g_ready_list);
}

int simple_scheduler_init() {
    INIT_LIST_HEAD(&g_ready_list);
    INIT_LIST_HEAD(&g_wait_list);

    return 0;
}