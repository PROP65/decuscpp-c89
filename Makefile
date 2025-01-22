#Tested with GNU make and bmake
CC = gcc
EXEC_NAME := decuscpp
CFLAGS := --std=c89 -pedantic -Wall $(FLAGS)

FILES = cpp1 cpp2 cpp3 cpp4 cpp5 cpp6

.PHONY: clean build

build:
	$(CC) $(CFLAGS) $(FILES:=.c) -o $(EXEC_NAME)

clean:
	rm -f $(EXEC_NAME)
