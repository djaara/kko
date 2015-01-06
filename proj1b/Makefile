#CFLAGS=-std=c99 -Wall -pedantic -ggdb3 -DDEBUG
CFLAGS=-std=c99 -Wall -pedantic -O2
CC=gcc
BINARY=gif2bmp
RM=rm -rf

all: main

main: main.o gif2bmp.o
		$(CC) $(CFLAGS) gif2bmp.o main.o -o $(BINARY)

debug: main.o gif2bmp.o
		$(CC) $(CFLAGS) gif2bmp.o main.o -o $(BINARY)

clean:
	$(RM) *.o $(BINARY)
