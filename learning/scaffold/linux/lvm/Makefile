# Makefile for LVM 演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = lvm

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
