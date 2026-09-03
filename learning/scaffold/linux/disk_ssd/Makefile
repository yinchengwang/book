# Makefile for SSD 特性演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = disk_ssd

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
