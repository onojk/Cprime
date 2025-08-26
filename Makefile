CC ?= gcc
CFLAGS ?= -O3 -march=native -Wall -Wextra -pipe
LDFLAGS ?=
LIBS = -lgmp

all: cprime

cprime: cprime.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LIBS)

clean:
	rm -f cprime
