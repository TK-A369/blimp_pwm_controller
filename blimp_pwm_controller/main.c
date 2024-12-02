#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>

#define CHANNELS 12

struct channel_entry {
	uint8_t channel;
	uint16_t duty;
};

volatile uint8_t twi_duties_counter = 0;
volatile uint8_t twi_duties_buf[CHANNELS];
volatile channel_entry channels_entries_double[CHANNELS * 2];
volatile channel_entry* channels_entries_front = channels_entries_double;
volatile channel_entry* channels_entries_back = channels_entries_double + CHANNELS;

ISR(TWI_vect) {
	switch(TWSR & 0b11111000) {
		case 0x60:
			// SLA+W received
			// Send ACK and receive byte
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
			break;
		case 0x80:
			// Data received
			// Send ACK and receive another byte
			twi_duties_buf[twi_duties_counter] = TWDR;
			twi_duties_counter++;
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
			break;
		case 0xA0:
			// STOP or REPEATED START received
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
			break;
		default:
			// Recover from error, go back to unaddressed slave mode
			
			for (uint8_t i = 0; i < CHANNELS; i++){
				channels_entries_back[i].channel = i;
				channels_entries_back[i].duty = twi_duties_buf[i];
			}
			
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE);
	}
}

ISR(TIMER1_OVF_vect) {
	// Set all the outputs
}

ISR(TIMER1_COMPA_vect) {
}

ISR(TIMER1_COMPB_vect) {
}

int main(void) {
	// Init TWI
	TWAR = (0x37 << 1);
	TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
	
	// Init Timer 1
	// Output compare pins disconnected
	// Mode: CTC, TOP=ICR1
	TCCR1A = 0;
	// Prescaler: clk_IO / 1
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS10);
	TCCR1C = 0;
	// Period: 20ms
	ICR1 = 20000;
	// Interrupts: overflow, output compare A and B
	TIMSK1 = (1 << OCIE1B) | (1 << OCIE1A) | (1 << TOIE1);
	
	sei();
	
    while (1) {
    }
}

