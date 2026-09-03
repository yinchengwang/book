# SQL DCL 事务学习演示 Makefile

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = sql_dcl
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
