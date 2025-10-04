#ifndef PS2_H
#define PS2_H

// Minimal PS/2 receive-only driver (device -> host) for AVR.
// Default pins/interrupts target ATmega32U4 (INT4 on PE4 = CLK, PE5 = DATA).
// You can override the pin/INT mapping via the macros below before including this header.

// -------------------- Configuration (override as needed) --------------------
#ifndef PS2_CLK_PORT
  #define PS2_CLK_PORT PORTE
#endif
#ifndef PS2_CLK_DDR
  #define PS2_CLK_DDR  DDRE
#endif
#ifndef PS2_CLK_PINR
  #define PS2_CLK_PINR PINE
#endif
#ifndef PS2_CLK_BIT
  // PE4 is INT4 on ATmega32U4
  #define PS2_CLK_BIT  PE4
#endif

#ifndef PS2_DAT_PORT
  #define PS2_DAT_PORT PORTE
#endif
#ifndef PS2_DAT_DDR
  #define PS2_DAT_DDR  DDRE
#endif
#ifndef PS2_DAT_PINR
  #define PS2_DAT_PINR PINE
#endif
#ifndef PS2_DAT_BIT
  // PE5 data
  #define PS2_DAT_BIT  PE5
#endif

// Which external interrupt and control bits to use (defaults: INT4 on 32U4)
#ifndef PS2_INT_MASK_REG
  #define PS2_INT_MASK_REG EIMSK
#endif
#ifndef PS2_INT_FLAG_REG
  #define PS2_INT_FLAG_REG EIFR
#endif
#ifndef PS2_INT_CTRL_REG
  // EICRA/EICRB split on many AVRs; INT4 uses EICRB
  #define PS2_INT_CTRL_REG EICRB
#endif
#ifndef PS2_INT_ENABLE_BIT
  #define PS2_INT_ENABLE_BIT INT4
#endif
#ifndef PS2_INT_FLAG_BIT
  #define PS2_INT_FLAG_BIT  INTF4
#endif
#ifndef PS2_ISC0_BIT
  // For INT4, the ISC bits are ISC40/ISC41 in EICRB
  #define PS2_ISC0_BIT ISC40
#endif
#ifndef PS2_ISC1_BIT
  #define PS2_ISC1_BIT ISC41
#endif

// Receive ring buffer size (must be power of two)
#ifndef PS2_RB_SIZE
  #define PS2_RB_SIZE 16
#endif

// If you want to use internal pull-ups instead of external 4.7k–10k,
// define PS2_USE_INTERNAL_PULLUPS to 1 before including this header.

// -------------------- API --------------------
#ifdef __cplusplus
extern "C" {
#endif

// Initialize PS/2 receiver:
// - Configures CLK/DAT as inputs (optionally enables internal pull-ups)
// - Sets interrupt on falling edge for CLK
// - Clears pending flag, enables the interrupt, and enables global interrupts (sei)
void ps2_init(void);

// Returns non-zero if at least one scan code byte is available.
int  ps2_available(void);

// Non-blocking read of one byte from the ring buffer.
// Caller should ensure ps2_available() > 0 before calling.
uint8_t ps2_read(void);


char ps2_get_char(void);



#ifdef __cplusplus
}
#endif

#endif // PS2_H
