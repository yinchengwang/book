# 树结构应用示例 - Makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = tree_applications
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
