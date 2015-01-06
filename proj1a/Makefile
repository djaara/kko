#CFLAGS=-std=c99 -Wall -pedantic -ggdb3 -DDEBUG
CFLAGS=-std=c99 -Wall -pedantic -O2
CC=gcc
BINARY=ahed
RM=rm -rf

all: main

main: main.o ahed.o
		$(CC) $(CFLAGS) ahed.o main.o -o $(BINARY)

debug: main.o ahed.o
		$(CC) $(CFLAGS) ahed.o main.o -o $(BINARY)

clean:
	$(RM) *.o $(BINARY)