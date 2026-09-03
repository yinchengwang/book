# TCP 拥塞控制演示 - Makefile
# 使用 C 语言演示 TCP 拥塞控制算法查询和 cwnd 概念

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = tcp_congestion
SRC = main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
