##############################################################################
#                                                                            #
# Change these to suit your hardware                                         #
#                                                                            #
##############################################################################

 MCU_TARGET = atmega328p

O = build

# CPU clock rate
F_CPU = 16000000UL

# application baud
APPBAUD = 250000
CONSOLE = /dev/arduino

DEFS = -DF_CPU=$(F_CPU) -DBAUD=$(APPBAUD)

##############################################################################
#                                                                            #
# Programmer settings for "make program"                                     #
#                                                                            #
##############################################################################

AVRDUDE = avrdude
AVRDUDECONF = /etc/avrdude.conf

##############################################################################
#                                                                            #
# udev rule for /dev/arduino (insert into /etc/udev/rules.d/99-local.rules)  #
# SUBSYSTEMS=="usb", ATTRS{idProduct}=="6001", ATTRS{idVendor}=="0403",      #
#     NAME="%k", SYMLINK+="arduino", SYMLINK+="arduino_$attr{serial}",       #
#     MODE="0660"                                                            #
#                                                                            #
##############################################################################

PROGPORT = /dev/arduino
PROGBAUD = 115200
PROGID   = arduino

##############################################################################
#                                                                            #
# These defaults should be ok, change if you need to                         #
#                                                                            #
##############################################################################

PROGRAM = exm-wind

SOURCES = $(wildcard *.c)

ARCH = avr-
CC = $(ARCH)gcc
OBJDUMP = $(ARCH)objdump
OBJCOPY = $(ARCH)objcopy

OPTIMIZE = -Os -ffunction-sections -finline-functions-called-once -mcall-prologues -maccumulate-args
CFLAGS  = -g -Wall -Wstrict-prototypes $(OPTIMIZE) -mmcu=$(MCU_TARGET) $(DEFS) -std=gnu11
CFLAGS += -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums -fno-stack-protector
LDFLAGS = -Wl,--as-needed -Wl,--gc-sections
LIBS = -lm
LIBDEPS =
SUBDIRS =

ifneq (,$(findstring usb,$(MCU_TARGET)))
LDFLAGS += -Llufa_serial
LIBS += -llufa_serial
SUBDIRS += lufa_serial
LIBDEPS += lufa_serial/liblufa_serial.a
else
SOURCES += serial.c
endif

OBJ = $(patsubst %.c,$(O)/%.o,${SOURCES})

.PHONY: all program console clean size subdirs program-fuses doc functionsbysize flash upload
.PRECIOUS: %.o %.elf

all: config.h subdirs $(O)/$(PROGRAM).hex $(O)/$(PROGRAM).lst size

$(O):
	@echo "  MKDIR     $@"
	@mkdir -p $@

$(O)/$(PROGRAM).elf: $(LIBDEPS) | $(O)

subdirs:
	@for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir; \
	done

program: $(O)/$(PROGRAM).hex
	$(AVRDUDE) -c$(PROGID) -b$(PROGBAUD) -p$(MCU_TARGET) -P$(PROGPORT) -C$(AVRDUDECONF) -U flash:w:$<

flash: program

upload: program

# anybaud: see https://gist.github.com/lategoodbye/f2d76134aa6c404cd92c because many linux installs can't do 250k
console:
	@stty raw ignbrk -echo <$(CONSOLE)
	@anybaud $(CONSOLE) $(APPBAUD)
	@cat $(CONSOLE)

$(O)/.lfuse: $(O)/fuse.o | $(O)
	avr-objcopy -j .fuse -I elf32-avr -O binary $< /dev/stdout | perl -ne '/(.)(.)(.)/ && printf "%c", ord $$1' > $@

$(O)/.hfuse: $(O)/fuse.o | $(O)
	avr-objcopy -j .fuse -I elf32-avr -O binary $< /dev/stdout | perl -ne '/(.)(.)(.)/ && printf "%c", ord $$2' > $@

$(O)/.efuse: $(O)/fuse.o | $(O)
	avr-objcopy -j .fuse -I elf32-avr -O binary $< /dev/stdout | perl -ne '/(.)(.)(.)/ && printf "%c", ord $$3' > $@

$(O)/.lock:  $(O)/fuse.o | $(O)
	avr-objcopy -j .lock -I elf32-avr -O binary $< /dev/stdout | perl -ne '/(.)/ && printf "%c", ord $$1' > $@

program-fuses: $(O)/.lfuse $(O)/.hfuse $(O)/.efuse $(O)/.lock
	$(AVRDUDE) -c$(PROGID) -b$(PROGBAUD) -p$(MCU_TARGET) -P$(PROGPORT) -C$(AVRDUDECONF) -U lfuse:w:.lfuse -U hfuse:w:.hfuse -U efuse:w:.efuse -U lock:w:.lock

clean: clean-subdirs
	@echo "  CLEAN     $(O)"
	@rm -rf $(OBJ) $(patsubst %.o,%.al,${OBJ}) $(O)/$(PROGRAM).{elf,hex,lst,sym} $(O)/.{l,h,e}fuse $(O)/.lock
	@if [ -d "$(O)" ]; then rmdir $(O); fi

clean-subdirs:
	@for dir in $(SUBDIRS); do \
	  $(MAKE) -C $$dir clean; \
	done

size: $(O)/$(PROGRAM).elf
	@echo
	@echo $$'            \033[1;4m SIZE        Atmega168        Atmega328p       Atmega644         Atmega1280        Atmega2560 \033[0m'
	@$(OBJDUMP) -h $^ | perl -MPOSIX -ne '/.(text)\s+([0-9a-f]+)/     && do { $$a += eval "0x$$2" }; END { printf "    FLASH : %5d bytes  %2d%% of %2dkb      %2d%% of %2dkb      %2d%% of %2dkb      %2d%% of %3dkb      %2d%% of %3dkb\n", $$a, ceil($$a * 100 / (15 * 1024)), 15, ceil($$a * 100 / (31 * 1024)), 31, ceil($$a * 100 / (63 * 1024)), 63, ceil($$a * 100 / (127 * 1024)), 127, ceil($$a * 100 / (255 * 1024)), 255 }'
	@$(OBJDUMP) -h $^ | perl -MPOSIX -ne '/.(data|bss)\s+([0-9a-f]+)/ && do { $$a += eval "0x$$2" }; END { printf "    RAM   : %5d bytes  %2d%% of %2dkb      %2d%% of %2dkb      %2d%% of %2dkb      %2d%% of %3dkb      %2d%% of %3dkb\n", $$a, ceil($$a * 100 / ( 1 * 1024)),  1, ceil($$a * 100 / ( 2 * 1024)),  2, ceil($$a * 100 / ( 4 * 1024)),  4, ceil($$a * 100 / (  8 * 1024)),   8, ceil($$a * 100 / (  8 * 1024)),   8 }'
	@$(OBJDUMP) -h $^ | perl -MPOSIX -ne '/.(eeprom)\s+([0-9a-f]+)/   && do { $$a += eval "0x$$2" }; END { printf "    EEPROM: %5d bytes  %2d%% of %2dkb      %2d%% of %2dkb      %2d%% of %2dkb      %2d%% of %3dkb      %2d%% of %3dkb\n", $$a, ceil($$a * 100 / ( 1 * 1024)),  1, ceil($$a * 100 / ( 2 * 1024)),  2, ceil($$a * 100 / ( 2 * 1024)),  2, ceil($$a * 100 / (  4 * 1024)),   4, ceil($$a * 100 / (  4 * 1024)),   4 }'

doc: Doxyfile *.c *.h
	doxygen $<

functionsbysize: $(OBJ)
	@avr-objdump -h $^ | grep '\.text\.' | perl -ne '/\.text\.(\S+)\s+([0-9a-f]+)/ && printf "%u\t%s\n", eval("0x$$2"), $$1;' | sort -n

$(O)/%.o: %.c config.h Makefile  | $(O)
	@echo "  CC        $@"
	@$(CC) -c $(CFLAGS) -Wa,-adhlns=$(@:.o=.al) -o $@ $<

$(O)/%.elf: $(OBJ) | $(O)
	@echo "  LINK      $@"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^ $(LIBS)

%.lst: %.elf
	@echo "  OBJDUMP   $@"
	@$(OBJDUMP) -h -S $< > $@

%.hex: %.elf
	@echo "  OBJCOPY   $@"
	@$(OBJCOPY) -j .text -j .data -O ihex -R .eeprom -R .fuse -R .lock $< $@

/%.bin: %.elf | $(O)
	@echo "  OBJCOPY   $@"
	@$(OBJCOPY) -j .text -j .data -O binary $< $@

/%.sym: %.elf | $(O)
	@echo "  SYM       $@"
	@$(OBJDUMP) -t $< | perl -ne 'BEGIN { printf "  ADDR  NAME                  SIZE\n"; } /([0-9a-f]+)\s+(\w+)\s+O\s+\.(bss|data)\s+([0-9a-f]+)\s+(\w+)/ && printf "0x%04x  %-20s +%d\n", eval("0x$$1") & 0xFFFF, $$5, eval("0x$$4")' | sort -k1 > $@
