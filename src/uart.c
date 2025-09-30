#include <avr/io.h>

#ifndef F_CPU
#define F_CPU 8000000UL
# error "Define F_CPU globally, e.g. add -DF_CPU=1000000UL to your CFLAGS"
#endif
#ifndef BAUD
# define BAUD 250000UL
#endif

static inline uint16_t ubrr_calc(void) {           // U2X = 1
    return (uint16_t)(F_CPU / (8UL * BAUD) - 1UL);
}

void uart_init(void) {
    const uint16_t ubrr = ubrr_calc();
    UCSR0A = _BV(U2X0);                             // double speed
    UBRR0H = (uint8_t)(ubrr >> 8);
    UBRR0L = (uint8_t)(ubrr & 0xFF);
    UCSR0B = _BV(TXEN0);                            // enable TX
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);             // 8N1
}

void uart_putc(char c) {                            // NOTE: not static
    while (!(UCSR0A & _BV(UDRE0))) { /* wait */ }
    UDR0 = c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}
