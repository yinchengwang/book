# Makefile for PQ Quantization Demo

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $< -lm

clean:
	rm -f $(TARGET)

.PHONY: all clean
