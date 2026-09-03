# Makefile for Vector Basic Demo

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -lm
TARGET = test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
