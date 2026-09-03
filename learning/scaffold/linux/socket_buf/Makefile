# Makefile for Linux Socket 缓冲区学习演示
# 编译：gcc -std=c11 -Wall -o socket_buf main.c

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := socket_buf

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
