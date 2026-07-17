# Makefile for ext4/xfs 演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = ext4_xfs

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
