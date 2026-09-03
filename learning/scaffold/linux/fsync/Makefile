# Makefile for fsync 演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = fsync

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) /tmp/fsync_test.bin

.PHONY: all clean
