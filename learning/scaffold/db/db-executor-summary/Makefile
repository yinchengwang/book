# Makefile for Executor Summary Demo

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = main
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

.PHONY: all clean