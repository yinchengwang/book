# Makefile for Linux TCP 状态学习演示
# 编译：gcc -std=c11 -Wall -o tcp_state main.c

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := tcp_state

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
