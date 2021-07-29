#ifndef FUSE_H
#define FUSE_H

#include <avr/io.h>
#include <avr/fuse.h>

#if defined (__AVR_ATmega328__) || defined (__AVR_ATmega328P__)
FUSES = {
	// CKSEL 0xD = Internal R/C osc @ 8MHz
    .low =      FUSE_CKSEL3 & FUSE_SUT0,
	// enable SPI programming, keep EEPROM contents when instructed to chip erase
    .high =     FUSE_SPIEN & FUSE_EESAVE,
	// BODLEVEL = 0x5 - BOD at 2.5 to 2.9v (2.7v nominal)
    .extended = FUSE_BODLEVEL1,
};
// nothing locked - no bootloader. program chip using ISP header
LOCKBITS = 0xFF;
// #else
// #error "Fuses invalid for this chip! Please see __FILE__ and enter appropriate settings, or check target!"
#endif

#endif /* FUSE_H */
