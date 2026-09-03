# Makefile for socket demo

CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -g
TARGET = socket_demo
SOURCES = main.c

.PHONY: all clean run run-server run-client

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)

run-server: all
	./$(TARGET) server

run-client: all
	./$(TARGET) client
