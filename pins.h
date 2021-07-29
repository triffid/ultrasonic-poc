#ifndef _PINS_H
#define _PINS_H

#include "arduino.h"

#define SERIAL_TX	PD1
#define SERIAL_RX	PD0

// atmega328p:
//
// AIN+ is PD6 - DIO6
// AIN- is PD7 - DIO7
// PB1 is DIO9
// PB2 is DIO10
// PB3 is DIO11

// both these pins must use the same port (PB, PC, PD) or we can't flip them both in unison
#define PING0		PB1
#define PING1		PB2

// any spare pin
#define COMPLEVEL   DIO5

// analog comparator inputs
#define AINP		PD6
#define AINN		PD7

#define LED			DIO13

#endif /* _PINS_H */
