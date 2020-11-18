LIB = libmspware

OBJECTS = \
	adc12_b.o \
	aes256.o \
	comp_e.o \
	crc32.o \
	crc.o \
	cs.o \
	dma.o \
	esi.o \
	eusci_a_spi.o \
	eusci_a_uart.o \
	eusci_b_i2c.o \
	eusci_b_spi.o \
	framctl.o \
	gpio.o \
	lcd_c.o \
	mpu.o \
	mpy32.o \
	pmm.o \
	ram.o \
	ref_a.o \
	rtc_b.o \
	rtc_c.o \
	sfr.o \
	sysctl.o \
	timer_a.o \
	timer_b.o \
	tlv.o \
	wdt_a.o \

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT)/include/$(LIB) \
