# learning/scaffold/ds/heap/Makefile
# 二叉堆 scaffold 编译脚本

CC       = gcc
CFLAGS   = -std=c11 -Wall -Wextra -O2
TARGET   = heap_demo
SRC      = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
