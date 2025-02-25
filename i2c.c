#include "i2c.h"

#include	<avr/io.h>
#include	<avr/interrupt.h>
#include	<util/twi.h>

#include	<stdio.h> // for NULL

#include	"pins.h"

#include	"serial.h"
#include	"sersendf.h"
#include	"fastio.h"

// #define F_I2C 100000UL
#define F_I2C 100000UL

static volatile i2c_transfer* current;
static uint8_t status_expect = TW_BUS_ERROR;
static uint8_t timeout = 255;

void process_transfer(void) {
	if (current == NULL) {
		// this should never happen, we're supposed to disable interrupts when sending stop
		// disable interrupt
		TWCR = MASK(TWINT) | MASK(TWEN);
		return;
	}

	i2c_transfer* c = (i2c_transfer*) current;

// 	sersendf_P(PSTR("P%pI%d:"), c, c->_i);

	uint8_t twcr = MASK(TWINT) | MASK(TWEN) | MASK(TWIE);

	if ((c->_stage_rd == 0) && (c->_stage_wr == 0)) {
		serial_writechar('S');
		twcr |= MASK(TWSTA); /* send start condition */
		status_expect = TW_START;
		c->_stage_wr = 1;
	}
	else if (c->_stage_wr) {
		if (c->_i >= c->wrsize) { // finished writing
			c->_stage_wr = 0;
			if (c->rdsize) { // if we have a read stage
				serial_writechar('A');
				c->_stage_rd = 1;
				c->_i = 0;
				twcr |= MASK(TWSTA); // send repeated start
				status_expect = TW_REP_START;
			}
			else { // move to next in queue (if any)
				serial_writechar('B');
				i2c_transfer* tmp = c;
				current = c->_next;

				if (current) {
					twcr |= MASK(TWSTA); // send start condition
					status_expect = TW_REP_START;
					current->_stage_wr = 1;
				}
				else {
					twcr |= MASK(TWSTO); // send stop condition
					twcr &= ~MASK(TWIE); // disable interrupt
					status_expect = TW_BUS_ERROR;
				}

				tmp->complete = 1;
				if (tmp->callback)
					tmp->callback(tmp);
			}
		}
		else {
			sersendf_P(PSTR("W%sx"), c->wrbuf[c->_i]);
			TWDR = c->wrbuf[c->_i];
			if (c->_i == 0)
				status_expect = TW_MT_SLA_ACK;
			else
				status_expect = TW_MT_DATA_ACK;
			/* twcr unchanged */ // start transmission
			c->_i++;
		}
	}
	else if (c->_stage_rd) {
		if (c->_i > (c->rdsize + 1)) { // finished reading
			serial_writechar('D');
			i2c_transfer* tmp = c;
			current = c->_next;

			if (current) {
				twcr |= MASK(TWSTA); // send start condition
				status_expect = TW_REP_START;
				current->_stage_wr = 1;
			}
			else {
				twcr |= MASK(TWSTO); // send stop condition
				twcr &= ~MASK(TWIE); // disable interrupt
				status_expect = TW_BUS_ERROR;
			}

			tmp->complete = 1;
			if (tmp->callback)
				tmp->callback(tmp);
		}
		else {
			if (c->_i == 0) { // write address to read from
				sersendf_P(PSTR("Q%sx"), c->wrbuf[0] | TW_READ);
				TWDR = c->wrbuf[0] | TW_READ;
				status_expect = TW_MR_SLA_ACK;
				/* twcr unchanged */ // start transmission
			}
			else if (c->_i == 1) { // address written, wait for data and issue ACKs
				twcr |= MASK(TWEA);
				status_expect = TW_MR_DATA_ACK;
			}
			else {
				uint8_t b = c->_i - 2;
// 				serial_writechar('F');
				uint8_t r = TWDR;
				c->rdbuf[b] = r;
				sersendf_P(PSTR("R%sx"), r);
				if (b < c->rdsize) {
					twcr |= MASK(TWEA); // ACK
					status_expect = TW_MR_DATA_ACK;
				}
				// else NAK
				else {
					status_expect = TW_MR_DATA_NACK;
				}
			}
			c->_i++;
		}
	}
	else {
		twcr &= ~MASK(TWIE);
		if (c) {
			i2c_transfer* tmp = c;
			current = c->_next;

			tmp->complete = 1;
			tmp->error    = 1;
			if (tmp->callback)
				tmp->callback(tmp);
		}
	}

	/*
	 * A5 = start
	 * 85 = data/nack
	 * B5 = stop+start
	 * C5 = ack
	 * 94 = stop (disable interrupt)
	 */
	sersendf_P(PSTR("[TW%sx]"), twcr);
	if ((twcr & MASK(TWIE)) == 0)
		serial_writechar('\n');

	TWCR = twcr;
}

ISR(TWI_vect) {
	/*
	 * 0x08 = OK  Start
	 * 0x10 = OK  Repeated start
	 * 0x18 = OK  SLA+W, acked
	 * 0x20 = ERR SLA+W, nak
	 * 0x28 = OK  data, acked
	 * 0x30 = ERR data, nak
	 * 0x38 = ERR SLA+R arbitration lost or NACK
	 * 0x40 = OK  SLA+R, acked
	 * 0x48 = ERR SLA+R, nak
	 * 0x50 = OK  data, acked
	 * 0x58 = OK  data, nak (ERR if we wanted to receive more bytes)
	 * 0xA8 = OK  SLA+R, acked
	 * 0xB0 = ERR SLA+RW arbitration lost
	 * 0xB8 = OK  data, acked
	 * 0xC0 = ERR data, nak
	 * 0xC8 = OK  last data, acked
	 */
	timeout = 255;
	uint8_t st = TW_STATUS;
	sersendf_P(PSTR(".[IS%sx/%sx]"), st, status_expect);

	if (st == status_expect) {
		process_transfer();
		return;
	}

	serial_writechar('Z');
	i2c_transfer* tmp = (i2c_transfer*) current;
	current = current->_next;

	// report TWI status and error flag
	// error flag is in bit 0 so it won't affect TWI status results which are in bits 3-7
	tmp->flags = st;
	tmp->error = 1;
	tmp->complete = 1;
	if (tmp->callback)
		tmp->callback(tmp);

	// if callback re-enqueues, start_transfer will issue a start condition

	uint8_t twcr = MASK(TWINT) | MASK(TWEN) | MASK(TWIE);

	if (current) {
		current->flags = 0;
		current->_stage_wr = 1;
		twcr |= MASK(TWSTO) | MASK(TWSTA);  // send stop+start condition, try next transfer
	}
	else {
		twcr &= ~MASK(TWIE);
		twcr |= MASK(TWSTO); // send stop condition, disable interrupt
	}

	TWCR = twcr;
}

i2c_transfer* start_transfer(i2c_transfer* t) {
	cli();
	if (current == NULL) {
		current = t;
		process_transfer();
		sei();
		return t;
	}
	sei();
	return NULL;
}

void i2c_init(void) {
	/*
	 * set up I2C
	 */
	TWBR = (((F_CPU / F_I2C) - 16UL) / 2UL) & 255U;
	TWCR = MASK(TWEN) | MASK(TWIE);
	TWSR = 0;

	current = NULL;
}

i2c_transfer* i2c_struct_init(i2c_transfer* t, void* wrbuf, uint8_t wrlength, void* rdbuf, uint8_t rdlength) {
	if (wrlength < 2)
		return NULL;
	t->wrbuf = (uint8_t*) wrbuf;
	t->wrsize = wrlength;
	t->rdsize = rdlength;
	if (rdlength)
		t->rdbuf = (uint8_t*) rdbuf;
	else
		t->rdbuf = NULL;
	t->flags = 0;
	t->_i = 0;
	t->_next = NULL;
	return t;
}

i2c_transfer* i2c_enqueue(i2c_transfer* t) {
	if (!t)
		return NULL;

	if (t == current)
		return NULL;

	if (t->wrsize < 2)
		return  NULL;

	t->_i = 0;
	t->flags = 0;
	t->_next = NULL;

	sersendf_P(PSTR("J%p"), current);
	cli();
	if (current == NULL) {
		sei();
		serial_writechar('N');
		return start_transfer(t);
	}
	else {
		serial_writechar('Q');
		i2c_transfer* w = (i2c_transfer*) current;
		while (w->_next)
			w = w->_next;
		w->_next = t;
		sei();
		return t;
	}
}

bool i2c_busy(void)
{
	cli();
	if (timeout == 0) {
		TWCR = 0;
		sei();
// 		serial_writechar('Z');
		i2c_transfer* tmp = (i2c_transfer*) current;
		if (tmp) {
			current = current->_next;

			// report TWI status and error flag
			// error flag is in bit 0 so it won't affect TWI status results which are in bits 3-7
			tmp->error = 1;
			tmp->complete = 1;
			if (tmp->callback)
				tmp->callback(tmp);
		}

		return false;
	}
	else {
		timeout--;
		sei();
	}

	if (TWCR & MASK(TWIE))
		return true;
	else if (current) {
		TWCR |= MASK(TWIE);
		return true;
	}

	return false;
}
