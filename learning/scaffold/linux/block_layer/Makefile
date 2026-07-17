# Makefile for 块设备层演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = block_layer

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
