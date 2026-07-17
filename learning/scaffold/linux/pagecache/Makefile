# Makefile for Linux 页缓存学习演示
# 编译：gcc -std=c11 -Wall -o pagecache main.c

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := pagecache

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
