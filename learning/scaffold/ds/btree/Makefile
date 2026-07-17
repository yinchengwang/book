# BTree Scaffold Makefile
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g
TARGET = btree_demo
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
