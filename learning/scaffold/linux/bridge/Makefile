# Makefile for Linux 网桥配置演示
CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = bridge_demo

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	sudo ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
