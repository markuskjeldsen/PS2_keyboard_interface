#define F_CPU 8000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>
#include "uart.h"
#include "ps2.h"

#define ADC_PRESCALER_BITS ( (0<<ADPS2) | (1<<ADPS1) | (1<<ADPS0)) //division factor of 8
#define ADC_REF_AVCC (1<<REFS0)


static inline void adc_init(void)
{
    // Select reference = AVCC, right-adjust result (ADLAR=0), start with channel 0
    ADMUX = ADC_REF_AVCC | 0;

    // Enable ADC, set prescaler
    ADCSRA = (1<<ADEN) | ADC_PRESCALER_BITS;

    // Optional: one dummy conversion after enabling ADC (improves first-read accuracy)
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & (1<<ADSC)) {;}
    ADCSRA |= (1<<ADIF); // clear flag by writing 1
}

// Do one blocking conversion on ADC channel 0..7 and return 10-bit result (0..1023)
uint16_t adc_read_single(uint8_t channel)
{
    if (channel > 7) channel = 7;                 // limit to single-ended channels
    // Keep reference bits, replace MUX bits
    uint8_t admux = (ADMUX & 0xE0) | (channel & 0x1F);
    ADMUX = admux;

    // Start conversion
    ADCSRA |= (1<<ADSC);

    // Wait for completion (ADSC clears to 0) or wait on ADIF
    while (ADCSRA & (1<<ADSC)) {;}

    // Read result: must read ADCL first, then ADCH (right-adjusted)
    uint8_t low  = ADCL;
    uint8_t high = ADCH;

    // Clear interrupt flag (optional here since ADSC method used)
    ADCSRA |= (1<<ADIF);

    return (uint16_t)high<<8 | low;
}



static int compute_top_and_prescaler(uint32_t freq_hz, uint16_t *top_out, uint8_t *cs_bits_out)
{
    if (freq_hz == 0) return -1;

    struct opt { uint16_t div; uint8_t cs; } opts[] = {
        {   1, (1U<<CS10) },
        {   8, (1U<<CS11) },
        {  64, (1U<<CS11) | (1U<<CS10) },
        { 256, (1U<<CS12) },
        {1024, (1U<<CS12) | (1U<<CS10) }
    };

    for (uint8_t i = 0; i < sizeof(opts)/sizeof(opts[0]); i++) {
        uint32_t top_calc = (F_CPU / ((uint32_t)opts[i].div * freq_hz));
        if (top_calc == 0) continue;
        top_calc -= 1U;
        if (top_calc >= 1U && top_calc <= 65535U) {
            *top_out = (uint16_t)top_calc;
            *cs_bits_out = opts[i].cs;
            return 0;
        }
    }
    return -1; // no valid prescaler/top combo found
}


int pwm1_init(uint32_t freq_hz)
{
    // Configure OC1A (PB5) as output
    DDRB |= (1U << DDB5);

    // Stop Timer1 while configuring
    TCCR1A = 0;
    TCCR1B = 0;

    // Fast PWM mode 14: WGM13:0 = 1110 (ICR1 as TOP)
    TCCR1A = (1U << WGM11);
    TCCR1B = (1U << WGM13) | (1U << WGM12);

    // Non-inverting mode on OC1A (clear on compare match, set at BOTTOM)
    TCCR1A |= (1U << COM1A1);

    uint16_t top;
    uint8_t cs_bits;
    if (compute_top_and_prescaler(freq_hz, &top, &cs_bits) != 0) {
        // Fallback to 20 kHz with prescaler 8 if possible
        freq_hz = 20000U;
        if (compute_top_and_prescaler(freq_hz, &top, &cs_bits) != 0) {
            return -1;
        }
    }

    // Set TOP (frequency) and start with 0% duty
    ICR1 = top;
    OCR1A = 0;

    // Start timer with selected prescaler
    TCCR1B |= cs_bits;

    return 0;
}

/*
 * Change PWM duty cycle on OC1A.
 * percent: 0..100 (%). Values above 100 will be clipped.
 */
void pwm1_set_duty_percent(uint8_t percent)
{
    if (percent > 100U) percent = 100U;

    uint16_t top = ICR1;
    // Scale duty based on TOP+1 (timer counts 0..TOP)
    uint32_t ocr = ((uint32_t)(top + 1U) * percent) / 100U;
    if (ocr > top) ocr = top;

    OCR1A = (uint16_t)ocr;
}

/*
 * Change PWM frequency at runtime while preserving duty percentage.
 * Returns 0 on success, -1 if requested frequency not achievable.
 */
int pwm1_set_frequency(uint32_t freq_hz)
{
    uint16_t old_top = ICR1;
    uint16_t old_ocr = OCR1A;

    // Compute current duty in permille to preserve better resolution
    uint32_t duty_permille = 0;
    if (old_top > 0) {
        duty_permille = ((uint32_t)old_ocr * 1000U) / (old_top + 1U);
        if (duty_permille > 1000U) duty_permille = 1000U;
    }

    uint16_t new_top;
    uint8_t new_cs;
    if (compute_top_and_prescaler(freq_hz, &new_top, &new_cs) != 0) {
        return -1;
    }

    // Atomically update prescaler and TOP to avoid glitches
    uint8_t sreg = SREG;
    cli();

    // Stop the clock (clear CS12:0), then set new TOP and prescaler
    TCCR1B &= ~((1U<<CS12) | (1U<<CS11) | (1U<<CS10));
    ICR1 = new_top;

    // Restore duty with new TOP using permille
    uint32_t new_ocr = ((uint32_t)(new_top + 1U) * duty_permille) / 1000U;
    if (new_ocr > new_top) new_ocr = new_top;
    OCR1A = (uint16_t)new_ocr;

    // Start with new prescaler
    TCCR1B |= new_cs;

    SREG = sreg;
    return 0;
}


int main(void){

	ps2_init();
	uart_init();
	uart_puts("\r\nPS/2 ready\r\n");

	for(;;){
		char c = ps2_get_char();

		if (c != 0) {
			uart_putc(c);
		}
        // Small idle delay is OK
        _delay_ms(10);
	}
}
    /*
    if(pwm1_init(20000U) == 0) {
	pwm1_set_duty_percent(40);
        }
    */
    //adc_init();
    //uint16_t sample = adc_read_single(3); // PF3
