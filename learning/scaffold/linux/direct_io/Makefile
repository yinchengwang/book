# Makefile for Direct I/O 演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = direct_io

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) /tmp/direct_io_test.bin

.PHONY: all clean
