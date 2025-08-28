CC=gcc
CFLAGS=-O3 -march=x86-64 -mtune=generic -pipe
LDLIBS=-lm

all: cprime_rho

cprime_rho: cprime_rho.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f cprime_rho *.o
.PHONY: all clean
