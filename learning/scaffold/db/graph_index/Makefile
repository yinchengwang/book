# Makefile for Graph Index Demo (NSW + HNSW)

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
