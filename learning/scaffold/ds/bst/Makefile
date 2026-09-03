# BST Scaffold Makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic
TARGET = bst_demo
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
