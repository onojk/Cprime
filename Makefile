CC      := gcc
CFLAGS  := -O3 -march=x86-64 -mtune=generic -pipe
LDLIBS  := -lgmp -lm

all: cprime_rho cprime_cli_demo

cprime_rho: cprime_rho.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

cprime_cli_demo: cprime.c
	$(CC) $(CFLAGS) -o $@ $< $(LDLIBS)

.PHONY: all clean
clean:
	rm -f cprime_rho cprime_cli_demo *.o
