# Makefile for Union-Find Scaffold
# 编译标准：C11

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -pedantic -O2
TARGET  := uf_demo
SRC     := main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
