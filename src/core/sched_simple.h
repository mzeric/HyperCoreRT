#pragma once


int  simple_scheduler_init();
void simple_scheduler_insert(hyper_task_t *task, struct list_head *head);
void simple_scheduler_yield(hyper_task_t *task);
void simple_scheduler_sched(hyper_task_t *task);
void simple_scheduler_block(hyper_task_t *task);
void simple_scheduler_unblock(hyper_task_t *task);
hyper_task_t *simple_scheduler_next();

extern struct list_head g_ready_list;