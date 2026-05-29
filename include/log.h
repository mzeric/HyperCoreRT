#pragma once

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Output mode for log_putchar */
enum log_output_mode {
    LOG_OUTPUT_UART   = 1 << 0,  /* write to UART */
    LOG_OUTPUT_BUF    = 1 << 1,  /* write to ring buffer */
    LOG_OUTPUT_BOTH   = LOG_OUTPUT_UART | LOG_OUTPUT_BUF,
};

/* Write one character through the log subsystem (ring buffer + UART). */
void log_putchar(char ch);

/* Flush ring buffer contents to UART. Typically called from panic. */
void log_flush(void);

/* Runtime log level control. */
void log_set_level(int level);
int  log_get_level(void);

/* Set output mode (UART only / buffer only / both). */
void log_set_output(unsigned int mode);

#ifdef __cplusplus
}
#endif
