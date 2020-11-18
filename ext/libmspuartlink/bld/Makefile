LIB = libmspuartlink

OBJECTS = \
	uartlink.o \

DEPS += \
	libmsp \
	libio \

include $(MAKER_ROOT)/Makefile.console

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT) \
	-I$(SRC_ROOT)/include/$(LIB) \
