LIB = libcapybara

include ../Makefile.config
include ../Makefile.options

OBJECTS = \
	capybara.o \
	power.o \

ifneq ($(LIBCAPYBARA_SWITCH_DESIGN),)
OBJECTS += reconfig.o
endif # LIBCAPYBARA_SWITCH_DESIGN

DEPS += \
	libmsp \


ifneq ($(LIBCAPYBARA_VARTH_ENABLED),)
DEPS += libmcppot
endif

override SRC_ROOT = ../../src

override CFLAGS += \
	-I$(SRC_ROOT)/include \
	-I$(SRC_ROOT)/include/$(LIB) \
