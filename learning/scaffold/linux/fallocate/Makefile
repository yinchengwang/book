# Makefile for fallocate 演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = fallocate

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) /tmp/fallocate_test.bin

.PHONY: all clean
