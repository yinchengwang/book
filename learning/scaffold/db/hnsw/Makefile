# Makefile for HNSW Index Demo
# HNSW（Hierarchical Navigable Small World）索引原理演示

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = test

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $< -lm

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
