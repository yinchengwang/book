# 线段树 scaffold Makefile
# 编译目标：演示程序

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
TARGET = segment_tree_demo
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
