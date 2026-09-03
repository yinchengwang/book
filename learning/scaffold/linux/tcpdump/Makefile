# Makefile for Linux tcpdump 原理学习演示
# 编译：gcc -std=c11 -Wall -o tcpdump main.c
#
# 注意：需要 root 权限运行
# sudo ./tcpdump

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := tcpdump

.PHONY: check clean all

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

# 运行测试（需要 root 权限）
check: $(TARGET)
	sudo ./$(TARGET)

# 清理编译产物
clean:
	rm -f $(TARGET)
