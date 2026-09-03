# Makefile for epoll demo

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
TARGET = epoll_demo
SOURCES = main.c

.PHONY: all clean run run-basic run-et run-compare

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

run-basic: all
	./$(TARGET) basic

run-et: all
	./$(TARGET) et

run-compare: all
	./$(TARGET) compare
