#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>
#include <stdint.h>

#define PS2_CLK_BIT   PE4        // INT4
#define PS2_DAT_BIT   PE5

#define PS2_CLK_PINR  PINE
#define PS2_DAT_PINR  PINE
#define PS2_DAT_MASK  (1<<PS2_DAT_BIT)

#define RB_SIZE 16
static volatile uint8_t rb[RB_SIZE];
static volatile uint8_t rhead, rtail;

typedef enum { RX_IDLE, RX_DATA, RX_PARITY, RX_STOP } rx_state_t;
static volatile rx_state_t rx_state = RX_IDLE;
static volatile uint8_t rx_byte;
static volatile uint8_t rx_bit_idx;  // 0..7
static volatile uint8_t rx_parity;   // running parity (odd)

// Timeout support: 1 MHz tick using Timer1 (Fcpu/8 at 8 MHz)
static inline uint16_t now_us(void){ return TCNT1; }
static volatile uint16_t last_edge_us;
#define FRAME_TIMEOUT_US 2000  // 2 ms

static inline void rb_push(uint8_t b){
    uint8_t nh = (rhead + 1) & (RB_SIZE - 1);
    if(nh != rtail){ rb[rhead] = b; rhead = nh; } // drop if full
}

static inline uint8_t rb_count(void){
    return (rhead - rtail) & (RB_SIZE - 1);
}
bool ps2_available(void){
    return rb_count() != 0;
}

uint8_t ps2_read(void){
	uint8_t b = rb[rtail];
	rtail = (rtail+1) & (RB_SIZE-1);
	return b;
}

static inline void rx_reset(void){
    rx_state   = RX_IDLE;
    rx_bit_idx = 0;
    rx_byte    = 0;
    rx_parity  = 1;
}


ISR(INT4_vect){
    // Inter-edge timeout: if too long since last falling edge, resync.
    uint16_t t = now_us();
    uint16_t dt = t - last_edge_us;  // 16-bit wrap is fine
    last_edge_us = t;
    if(dt > FRAME_TIMEOUT_US){
        rx_reset();
    }

    uint8_t bit = (PS2_DAT_PINR & PS2_DAT_MASK) ? 1 : 0;

    switch(rx_state){
    case RX_IDLE:
        // Falling edge while DATA is 0 -> start bit
        if(bit == 0){
            rx_state   = RX_DATA;
            rx_bit_idx = 0;
            rx_byte    = 0;
            rx_parity  = 1; // odd parity tracking
        }
        break;

    case RX_DATA:
        // LSB first
        if(bit) rx_byte |= (1u << rx_bit_idx);
        rx_parity ^= bit;
        if(++rx_bit_idx >= 8){
            rx_state = RX_PARITY;
        }
        break;

    case RX_PARITY:
        // For odd parity, parity bit must equal rx_parity
        if(bit != rx_parity){
            rx_reset();            // parity error -> resync
        }else{
            rx_state = RX_STOP;
        }
        break;

    case RX_STOP:
        // Stop must be 1
        if(bit == 1){
            rb_push(rx_byte);
        }
        rx_reset();  // good or bad stop, resync
        break;
    }
}

void ps2_init(void){
    // Configure inputs
    DDRE &= ~((1<<PS2_CLK_BIT)|(1<<PS2_DAT_BIT));

    // If you have external pull-ups, prefer disabling the internal ones:
    // PORTE &= ~((1<<PS2_CLK_BIT)|(1<<PS2_DAT_BIT));
    // If you don't have externals, use the internals (works in practice):
    // PORTE |= (1<<PS2_CLK_BIT)|(1<<PS2_DAT_BIT);

    // Timer1: 1 MHz tick (assuming F_CPU = 8 MHz). Prescaler 8.
    TCCR1A = 0;
    TCCR1B = (1<<CS11); // clk/8
    TCNT1 = 0;
    last_edge_us = 0;

    // External interrupt INT4 on falling edge: ISC41=1, ISC40=0
    EICRB = (EICRB & ~((1<<ISC41)|(1<<ISC40))) | (1<<ISC41);
    EIFR  = (1<<INTF4);   // clear any pending
    EIMSK |= (1<<INT4);   // enable INT4

    rx_reset();
    rhead = rtail = 0;
    sei();
}

char uart_print_ps2_scancodes(uint8_t input) {
    switch (input) {
        case 0x76: return 27; // ESC
        case 0x05: return 133; // F1
        case 0x06: return 134; // F2
        case 0x04: return 135; // F3
        case 0x0C: return 136; // F4
        case 0x03: return 137; // F5
        case 0x0B: return 138; // F6
        case 0x83: return 139; // F7
        case 0x0A: return 140; // F8
        case 0x01: return 141; // F9
        case 0x09: return 142; // F10
        case 0x78: return 143; // F11
        case 0x07: return 144; // F12
        case 0x7E: return 146; // Scroll Lock
        case 0x0E: return '`';
        case 0x16: return '1';
        case 0x1E: return '2';
        case 0x26: return '3';
        case 0x25: return '4';
        case 0x2E: return '5';
        case 0x36: return '6';
        case 0x3D: return '7';
        case 0x3E: return '8';
        case 0x46: return '9';
        case 0x45: return '0';
        case 0x4E: return '-';
        case 0x55: return '=';
        case 0x66: return '\b'; // Backspace
        case 0x0D: return '\t'; // Tab
        case 0x15: return 'q';
        case 0x1D: return 'w';
        case 0x24: return 'e';
        case 0x2D: return 'r';
        case 0x2C: return 't';
        case 0x35: return 'y';
        case 0x3C: return 'u';
        case 0x43: return 'i';
        case 0x44: return 'o';
        case 0x4D: return 'p';
        case 0x54: return '[';
        case 0x5B: return ']';
        case 0x5D: return '\\';
        case 0x58: return 145; // Caps Lock
        case 0x1C: return 'a';
        case 0x1B: return 's';
        case 0x23: return 'd';
        case 0x2B: return 'f';
        case 0x34: return 'g';
        case 0x33: return 'h';
        case 0x3B: return 'j';
        case 0x42: return 'k';
        case 0x4B: return 'l';
        case 0x4C: return ';';
        case 0x52: return '\'';
        case 0x5A: return '\n'; // Enter
        case 0x12: return 128; // Left Shift
        case 0x1A: return 'z';
        case 0x22: return 'x';
        case 0x21: return 'c';
        case 0x2A: return 'v';
        case 0x32: return 'b';
        case 0x31: return 'n';
        case 0x3A: return 'm';
        case 0x41: return ',';
        case 0x49: return '.';
        case 0x4A: return '/';
        case 0x59: return 129; // Right Shift
        case 0x14: return 130; // Left Ctrl
        case 0x11: return 132; // Left Alt
        case 0x29: return ' '; // Spacebar
        case 0x77: return 147; // Num Lock
        case 0x7C: return '*'; // Keypad *
        case 0x7B: return '-'; // Keypad -
        case 0x6C: return '7'; // Keypad 7
        case 0x75: return '8'; // Keypad 8
        case 0x7D: return '9'; // Keypad 9
        case 0x79: return '+'; // Keypad +
        case 0x6B: return '4'; // Keypad 4
        case 0x73: return '5'; // Keypad 5
        case 0x74: return '6'; // Keypad 6
        case 0x69: return '1'; // Keypad 1
        case 0x72: return '2'; // Keypad 2
        case 0x7A: return '3'; // Keypad 3
        case 0x70: return '0'; // Keypad 0
        case 0x71: return '.'; // Keypad .
        default: return 0; // Return 0 for unhandled scancodes
    }
}



char uart_print_ps2_extended(uint8_t input) {
    switch (input) {
        case 0x1F: return 131; // Left Windows
        case 0x11: return 132; // Right Alt
        case 0x27: return 131; // Right Windows
        case 0x2F: return 150; // Menus
        case 0x14: return 130; // Right Ctrl
        case 0x70: return 153; // Insert
        case 0x6C: return 154; // Home
        case 0x7D: return 155; // Page Up
        case 0x71: return 156; // Delete
        case 0x69: return 157; // End
        case 0x7A: return 158; // Page Down
        case 0x75: return 159; // Up Arrow
        case 0x6B: return 160; // Left Arrow
        case 0x72: return 161; // Down Arrow
        case 0x74: return 162; // Right Arrow
        case 0x4A: return '/';   // Keypad /
        case 0x5A: return '\n';  // Keypad Enter
        default: return 0; // Return 0 for unhandled scancodes
    }
}


char ps2_get_char(void) {
    // These flags MUST be static to remember the state between calls.
    static uint8_t is_extended = 0;
    static uint8_t is_break = 0;

    // This function will process all available bytes until it can resolve
    // a character or the buffer is empty.
    while (ps2_available()) {
        uint8_t scancode = ps2_read();

        // --- State Detection ---
        if (scancode == 0xE0) {
            is_extended = 1;
            continue; // Prefix code, so get the next byte.
        } else if (scancode == 0xF0) {
            is_break = 1;
            continue; // Prefix code, so get the next byte.
        }

        // --- Scancode Processing ---
        if (is_break) {
            // This is a "key up" event. We don't need to do anything with it
            // for this application, so we just reset the flag and move on.
            is_break = 0;
            continue; // Go to the next byte in the buffer if there is one.
        }

        char character;
        if (is_extended) {
            character = uart_print_ps2_extended(scancode);
            is_extended = 0; // Reset the flag
        } else {
            character = uart_print_ps2_scancodes(scancode);
        }

        // If we successfully translated a character, return it immediately.
        if (character != 0) {
            return character;
        }
    }

    // If we've processed everything in the buffer and haven't returned a
    // character (e.g., we only saw break codes), return 0.
    return 0; // 0 represents "no character available".
}
