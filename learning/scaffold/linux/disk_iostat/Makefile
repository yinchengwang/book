# Makefile for Linux 磁盘 I/O 诊断学习演示
# 编译：gcc -std=c11 -Wall -o disk_iostat main.c

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := disk_iostat

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
