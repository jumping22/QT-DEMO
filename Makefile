# UART loop TX/RX test for RK3568 (/dev/ttyS9)
#
# Native build on board:
#   make
#
# Cross build (host with aarch64 toolchain):
#   make CROSS_COMPILE=aarch64-linux-gnu-

CROSS_COMPILE ?=
CC      := $(CROSS_COMPILE)gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11
LDFLAGS ?= -pthread

TARGET  := uart_loop_test
SRCS    := uart_loop_test.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET)
