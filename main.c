#include	<avr/interrupt.h>

#include	<stdint.h>
#include	<stdlib.h>
#include 	<stdio.h>
#include	<stdbool.h>

#include	"fastio.h"
#include	"serial.h"

// arduino uno:
//
// AIN+ is PD6 - DIO6 - connect this to the receiver
// AIN- is PD7 - DIO7 - connect this to the threshold selector
// PB1 is DIO9        - connect this to the transmitter
// PB2 is DIO10       - connect this to the transmitter's other leg

// pins for TX driver
#define PING0		PB1
#define PING1		PB2

// any spare pin
#define COMPLEVEL   DIO5

#define LED			DIO13

#ifndef	MASK
	#define	MASK(m)	(1UL << m)
#endif

#define	DEBUG 0

#define INTERVAL 250

#define diff(a,b)	(((a) >= (b))?((a)-(b)):((b)-(a)))

inline bool ge(uint32_t a, uint32_t b) {
	return ((a - b) & 0x80000000UL) == 0;
}

inline bool lt(uint32_t a, uint32_t b) {
	return ((a - b) & 0x80000000UL) != 0;
}

volatile uint32_t milliseconds = 0;

uint16_t starttime = 0;
volatile uint16_t envelopetime = 0;
volatile uint16_t phasetime = 0;

#define PULSECOUNT_X2 16
volatile uint8_t edges = 0;

void pins_init(void) {
	WRITE(PING0, 0);
	SET_OUTPUT(PING0);

	WRITE(PING1, 0);
	SET_OUTPUT(PING1);

	WRITE(LED, 0);
	SET_OUTPUT(LED);

	WRITE(COMPLEVEL, 0);
	SET_OUTPUT(COMPLEVEL);
}

void timer_init(void) {
	// init timers
	GTCCR = MASK(TSM) | MASK(PSRSYNC); // pause clocks

	PRR &= ~MASK(PRTIM1) & ~MASK(PRTIM0) & ~MASK(PRADC); // enable timers 0 and 1 and ADC

	// use timer1 for output waveform generation and analog comparator timing
	TCCR1A = 0;                       // normal mode, no match behaviour
	TCCR1B = MASK(CS10);              // maximum speed (no prescale)

	// use timer0 for our millisecond clock
	TCCR0A = MASK(WGM01);             // CTC mode
	TCCR0B = MASK(CS00) | MASK(CS01); // prescaler=64, ie clock is 250kHz
	OCR0A  = 250;                     // interrupt at 1kHz, ie every 1ms
	TIMSK0 = MASK(OCIE0A);            // interrupt on compare

	// set up analog comparator
	ADCSRB &= ~MASK(ACME);               // disable mux
	ACSR    = MASK(ACO) | MASK(ACIC);  // rising edge
	DIDR1   = MASK(AIN1D) | MASK(AIN0D); // turn off digital inputs on comparator pins

	GTCCR = 0; // re-enable clocks
}

int main(void) {
	pins_init();

	serial_init();

	timer_init();

	sei();

	puts_P(PSTR("Start\n"));

	uint32_t pingtime = INTERVAL;

	for (;;) {
		if (ge(milliseconds, pingtime) && (edges == 0)) {
			// note when we should ping next
			pingtime += INTERVAL;

			// start in a millisecond from now to avoid race conditions
			starttime = TCNT1 + (F_CPU / 1000);
			OCR1A = OCR1B = starttime;

			// reset edge counter
			edges = PULSECOUNT_X2;

			// set A and clear B on match
			TCCR1A = MASK(COM1A0) | MASK(COM1A1) | MASK(COM1B1);

			// set comparator level to 1/3 vcc
			SET_OUTPUT(COMPLEVEL);

			// disable interrupt on comparator
			TIMSK1 &= ~MASK(ICIE1);

			// clear flag in case it triggered during startup
			TIFR1 = MASK(OCF1A);

			// enable interrupt
			TIMSK1 |= MASK(OCIE1A);
		}
		if (phasetime != 0) {
			uint16_t eoff = envelopetime - starttime; // time when Rx signal became loud enough to consider, ie start of envelope
			uint16_t poff = phasetime - starttime;    // time of first zero crossing after envelope, use for calculating phase offset

			printf_P(PSTR("envelope +%u (%uus) phase +%u (%uus)\n"), eoff, eoff >> 4, poff, poff >> 4);

			// zero so we only print once, process data before this
			phasetime = 0;
		}
	}
}

ISR(TIMER1_COMPA_vect) {
	if (edges == PULSECOUNT_X2) {
		TCCR1B |= MASK(ICES1);   // listen for rising edge
		TIFR1 = MASK(ICF1);      // clear flags in case it triggered during startup
		TIMSK1 |= MASK(ICIE1);   // interrupt on comparator

		TCCR1A = MASK(COM1A0) | MASK(COM1B0); // toggle on match
		WRITE(LED, 1);
	}
	else if (edges == 1) {
		TCCR1A = MASK(COM1A1) | MASK(COM1B1); // clear both on match
	}
	else if (edges == 0) {
		putc('>');

		TIMSK1 &= ~MASK(OCIE1A);   // disable interrupt

		return;
	}
	edges--;
	OCR1A = OCR1B += F_CPU / 80000; // set next timeout
}

ISR(TIMER1_CAPT_vect) {
	putc('!');
	if (TCCR1B & MASK(ICES1)) {
		TCCR1B &= ~MASK(ICES1);   // listen for falling edge
		SET_INPUT(COMPLEVEL);     // set comparator level to 1/2 vcc
		envelopetime = ICR1;      // store timestamp
		TIFR1 = MASK(ICF1);
	}
	else {
		WRITE(LED, 0);
		TIMSK1 &= ~MASK(ICIE1);   // disable interrupt on comparator
		phasetime = ICR1;         // store timestamp
	}
}

ISR(TIMER0_COMPA_vect) {
	milliseconds++;
}
