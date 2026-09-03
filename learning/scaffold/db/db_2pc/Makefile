# 两阶段提交（2PC）演示程序 Makefile

CC = gcc
CFLAGS = -std=c11 -Wall
TARGET = 2pc
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
