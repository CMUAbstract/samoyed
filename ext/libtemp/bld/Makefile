LIB = libtemp

OBJECTS = \
	temp.o \


# libmsp is a dep only to not duplicate REF settling time param
DEPS += \
	libmsp \

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \
