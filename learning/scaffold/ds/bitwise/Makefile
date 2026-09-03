# Makefile for 数据结构 - 位运算学习卡片

CC := gcc
CFLAGS := -std=c11 -Wall
TARGET := test

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
