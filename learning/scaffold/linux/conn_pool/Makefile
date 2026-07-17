# Makefile for Linux 连接池学习演示
# 编译：gcc -std=c11 -Wall -o conn_pool main.c -lpthread

CC := gcc
CFLAGS := -std=c11 -Wall
LDFLAGS := -lpthread
TARGET := conn_pool

.PHONY: check clean all

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

# 运行测试
check: $(TARGET)
	./$(TARGET)

# 清理编译产物
clean:
	rm -f $(TARGET)
