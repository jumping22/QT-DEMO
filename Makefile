CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
LDFLAGS ?= -lm

.PHONY: all test clean

all: smooth

smooth: main.c smooth.c smooth.h
	$(CC) $(CFLAGS) -o $@ main.c smooth.c $(LDFLAGS)

test: smooth
	./smooth --self-test
	./smooth --csv samples.txt > /tmp/smooth.csv
	./smooth --svg /tmp/smooth.svg samples.txt >/dev/null
	@echo "ok"

clean:
	rm -f smooth
