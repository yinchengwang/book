# Makefile for I/O 调度器演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = io_scheduler

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
