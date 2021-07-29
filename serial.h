#ifndef	_SERIAL_H
#define	_SERIAL_H

#include	<stdint.h>
#include	<avr/io.h>
#include	<avr/pgmspace.h>

// initialise serial subsystem
void serial_init(void);

// return number of characters in the receive buffer, and number of spaces in the send buffer
uint8_t serial_rxchars(void);
// uint8_t serial_txchars(void);

// read one character
uint8_t serial_popchar(void);
// send one character
void serial_writechar(uint8_t data);

// wait for txqueue to empty
void serial_flush(void);

// read/write many characters
// uint8_t serial_recvblock(uint8_t *block, int blocksize);
void serial_writeblock(const void *data, int datalen);

void serial_writestr(const uint8_t *data);

// write from flash
void serial_writeblock_P(PGM_P data, int datalen);
void serial_writestr_P(PGM_P data);

#include "sermsg.h"
#include "sersendf.h"

#undef putc
#define putc(c) serial_writechar(c)
#define puts_P(s) serial_writestr_P(s)
#define printf_P(...) sersendf_P(__VA_ARGS__)

#endif	/* _SERIAL_H */
