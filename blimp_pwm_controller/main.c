#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdint.h>
#include <string.h>

#define CHANNELS 12
// PC0-4, PD0-7

struct channel_entry {
	uint8_t channel;
	uint16_t duty;
};

volatile uint8_t twi_duties_counter = 0;
volatile uint16_t twi_duties_buf[CHANNELS];
volatile struct channel_entry channels_entries_double[CHANNELS * 2];
volatile struct channel_entry* channels_entries_front = channels_entries_double;
volatile struct channel_entry* channels_entries_back = channels_entries_double + CHANNELS;
volatile uint8_t channels_updated = 0;
volatile uint8_t next_channel_entry = 0;
volatile uint8_t* outputs_ports[] = {&PORTC, &PORTD, &PORTD}; // Divide output number by 4 and look up from this array
uint8_t outputs_pins[] = {0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7};

ISR(TWI_vect) {
	uint8_t twdr_tmp;
	switch(TWSR & 0b11111000) {
		case 0x60:
			// SLA+W received
			// Send ACK and receive byte
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
			break;
		case 0x80:
			// Data received
			// Send ACK and receive another byte
			twdr_tmp = TWDR;
			// Map from 0-255 to 1000-2000
			twi_duties_buf[twi_duties_counter] = (((uint32_t)twdr_tmp) * 1000 / 255) + 1000;
			twi_duties_counter++;
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
			break;
		case 0xA0:
			// STOP or REPEATED START received
			
			for (uint8_t i = 0; i < CHANNELS; i++) {
				uint8_t j = i;
				while (j > 0) {
					if (channels_entries_back[j - 1].duty > twi_duties_buf[i]) {
						j--;
					}
					else {
						break;
					}
				}
				memmove((struct channel_entry*)(channels_entries_back + j + 1), (const struct channel_entry*)(channels_entries_back + j), (i - j) * sizeof(struct channel_entry));
				channels_entries_back[j].channel = i;
				channels_entries_back[j].duty = twi_duties_buf[i];
			}
			channels_updated = 1;
			
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
			break;
		default:
			// Recover from error, go back to unaddressed slave mode
			TWCR = (1 << TWINT) | (1 << TWEA) | (1 << TWSTO) | (1 << TWEN) | (1 << TWIE);
	}
}

ISR(TIMER1_CAPT_vect) {
	//PORTC ^= (1 << 0);
	
	// Set all the outputs
	PORTC |= 0x0F;
	PORTD = 0xFF;
	
	if (channels_updated) {
		channels_updated = 0;
		volatile struct channel_entry* tmp = channels_entries_front;
		channels_entries_front = channels_entries_back;
		channels_entries_back = tmp;
	}
	
	next_channel_entry = 0;
	OCR1A = channels_entries_front[next_channel_entry].duty;
}

ISR(TIMER1_COMPA_vect) {
	//PORTC ^= (1 << 0);
	
	uint16_t tcnt1_buf = TCNT1;
	do {
		uint8_t chan = channels_entries_front[next_channel_entry].channel;
		*(outputs_ports[chan >> 2 /* divide by 4 */]) &=~ (1 << outputs_pins[chan]);
		
		next_channel_entry++;
		tcnt1_buf += 10;
	} while (next_channel_entry < CHANNELS && channels_entries_front[next_channel_entry].duty < tcnt1_buf);
	
	if (next_channel_entry < CHANNELS) {
		OCR1A = channels_entries_front[next_channel_entry].duty;
	}
}

int main(void) {
	// Init GPIO
	DDRC |= 0x0F;
	DDRD = 0xFF;
	
	// Init TWI
	TWAR = (0x37 << 1);
	TWCR = (1 << TWEA) | (1 << TWEN) | (1 << TWIE);
	
	// Init Timer 1
	// Output compare pins disconnected
	// Mode: CTC, TOP=ICR1
	TCCR1A = 0;
	// Prescaler: clk_IO / 8
	TCCR1B = (1 << WGM13) | (1 << WGM12) | (1 << CS11);
	TCCR1C = 0;
	// Period: 20ms
	ICR1 = 20000;
	OCR1A = 10000;
	// Interrupts: TOP=ICR1 reached, output compare A
	TIMSK1 = (1 << ICIE1) | (1 << OCIE1A);
	
	channels_entries_back[0].channel = 0;
	channels_entries_back[0].duty = 1000;
	channels_entries_back[1].channel = 1;
	channels_entries_back[1].duty = 1000;
	channels_entries_back[2].channel = 2;
	channels_entries_back[2].duty = 1100;
	channels_entries_back[3].channel = 3;
	channels_entries_back[3].duty = 1100;
	channels_entries_back[4].channel = 4;
	channels_entries_back[4].duty = 1100;
	channels_entries_back[5].channel = 5;
	channels_entries_back[5].duty = 1100;
	channels_entries_back[6].channel = 6;
	channels_entries_back[6].duty = 1300;
	channels_entries_back[7].channel = 7;
	channels_entries_back[7].duty = 1300;
	channels_entries_back[8].channel = 8;
	channels_entries_back[8].duty = 1300;
	channels_entries_back[9].channel = 9;
	channels_entries_back[9].duty = 1500;
	channels_entries_back[10].channel = 10;
	channels_entries_back[10].duty = 1600;
	channels_entries_back[11].channel = 11;
	channels_entries_back[11].duty = 2000;
	channels_updated = 1;
	
	sei();
	
    while (1) {
		//PORTC ^= (1 << 0);
    }
}

