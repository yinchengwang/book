# Makefile for 乐观并发控制演示
CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -O2
TARGET  = optimistic
SRC     = main.c

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET)
