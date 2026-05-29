#pragma once

/* Board */
#define CONFIG_BOARD_QEMU_VIRT 1

/* SMP */
#define CONFIG_SMP_CPU_NUM 4

/* Memory */
#define CONFIG_PAGE_SHIFT 12
#define CONFIG_PADDR_BITS 44
#define CONFIG_PHY_MEM_SIZE 0x80000000

/* Load addresses */
#define CONFIG_ENTRY_ADDR 0x40080000
#define CONFIG_DTB_LOAD_PHYS_ADDR 0x40000000
#define CONFIG_GUEST_OS_LOAD_ADDR 0x50200000

/* Stack */
#define CONFIG_INT_STACK_SIZE 409600

/* Log levels */
#define LOG_LEVEL_DEBUG  0
#define LOG_LEVEL_INFO   1
#define LOG_LEVEL_WARN   2
#define LOG_LEVEL_ERR    3
#define LOG_LEVEL_FATAL  4
#define LOG_LEVEL_NONE   5

/* Compile-time minimum log level (lower levels compiled out entirely) */
#ifndef LOG_LEVEL
#define LOG_LEVEL  LOG_LEVEL_INFO
#endif

/* Ring buffer size for log retention */
#define LOG_BUF_SIZE  (1 << 16)  /* 64KB */
