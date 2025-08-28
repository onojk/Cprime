# Simple Makefile for cprime_cli_demo + tests

CC      = cc
CFLAGS  = -O3 -march=native -Wall -Wextra -pipe

all: cprime_cli_demo

cprime_cli_demo: Script1.c
	$(CC) $(CFLAGS) Script1.c -o cprime_cli_demo

test: cprime_cli_demo smoketest_fast.sh
	./smoketest_fast.sh

clean:
	rm -f cprime_cli_demo Script1.o
