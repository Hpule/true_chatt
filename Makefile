# Makefile for CPE464 tcp test code
# written by Hugh Smith - April 2019

CC = gcc
CFLAGS = -g -Wall -std=gnu99
LIBS = 

# Object files
OBJS = networks.o gethostbyname.o pollLib.o safeUtil.o pdu.o handleTable.o

all: cclient server

cclient: cclient.c $(OBJS)
	$(CC) $(CFLAGS) -o cclient cclient.c $(OBJS) $(LIBS)

server: server.c $(OBJS)
	$(CC) $(CFLAGS) -o server server.c $(OBJS) $(LIBS)

# Generic rule for building object files from C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@ $(LIBS)

cleano:
	rm -f *.o

clean:
	rm -f server cclient *.o
