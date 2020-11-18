LIB = libio

OBJECTS = \
	empty.o \

ifneq ($(LIBIO_BACKEND),)
OBJECTS += \
	printf.o \
	console_$(LIBIO_BACKEND).o
endif

override SRC_ROOT = ../../src
