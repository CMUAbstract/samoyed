override BOARD = capybara
export BOARD

export BOARD_MAJOR = 2
export BOARD_MINOR = 0

TOOLCHAINS = \
	gcc \
	chain \
	jit \

SYSTEM ?= null
APP ?= null

include ext/maker/Makefile
export CHAIN_ROOT = $(LIB_ROOT)/chain
export JIT_ROOT = $(LIB_ROOT)/jit

# Paths to toolchains here if not in or different from defaults in Makefile.env
