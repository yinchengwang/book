# 跳表（Skiplist）Scaffold Makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic
TARGET = skiplist_demo
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
