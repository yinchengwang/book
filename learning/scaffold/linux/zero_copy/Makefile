# Makefile for zero_copy demo

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
TARGET = zero_copy_demo
SOURCES = main.c

.PHONY: all clean run run-compare

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET) /tmp/zero_copy_test.bin

run: all
	./$(TARGET)

run-compare: all
	./$(TARGET) compare
