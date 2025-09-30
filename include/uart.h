#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);  // optional helper

#ifdef __cplusplus
}
#endif
