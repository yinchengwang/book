# Makefile for seccomp 演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = seccomp

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
