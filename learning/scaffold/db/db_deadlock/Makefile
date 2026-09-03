# 死锁处理学习演示 - Makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pedantic
TARGET = deadlock
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
