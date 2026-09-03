# Makefile for SQLite VSS Extension Concepts Demo
# SQLite 向量扩展（VSS）概念演示

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -lm
TARGET = test.exe

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

.PHONY: all clean
