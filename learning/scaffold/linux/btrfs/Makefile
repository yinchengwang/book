# Makefile for Linux Btrfs 文件系统 学习演示
# 编译：gcc -std=c11 -Wall -o btrfs main.c

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := btrfs

.PHONY: check clean all

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

# 运行测试
check: $(TARGET)
	./$(TARGET)

# 清理编译产物
clean:
	rm -f $(TARGET)
