# Binary Tree Scaffold Makefile
# 使用 gcc 编译，C11 标准

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = binarytree
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
