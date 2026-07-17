# Makefile for Linux CFS 调度器学习演示
# 编译：gcc -std=c11 -Wall -o cfs main.c

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := cfs

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
