# Makefile for Linux 文件系统日志 学习演示
# 编译：gcc -std=c11 -Wall -o fs_journal main.c

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := fs_journal

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
