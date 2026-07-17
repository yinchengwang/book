# Makefile for fdatasync 演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = fdatasync

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) /tmp/fdatasync_test.bin

.PHONY: all clean
