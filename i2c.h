#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stdbool.h>

typedef struct _i2c_transfer i2c_transfer;

typedef void (*i2c_callback)(i2c_transfer*);

struct _i2c_transfer {
	uint8_t _i;
	union {
		volatile uint8_t flags;
		struct {
			volatile uint8_t error    :1;
			volatile uint8_t complete :1;

			volatile uint8_t _stage_wr :1;
			volatile uint8_t _stage_rd :1;
		};
	};

	uint8_t  wrsize;
	uint8_t  rdsize;

	uint8_t* wrbuf;
	uint8_t* rdbuf;

	i2c_callback callback;

	i2c_transfer* _next;
};

void i2c_init(void);

i2c_transfer* i2c_struct_init(i2c_transfer*, void* wrbuf, uint8_t wrlength, void* rdbuf, uint8_t rdlength);

i2c_transfer* i2c_enqueue(i2c_transfer*);

bool i2c_busy(void);

#endif /* I2C_H */
