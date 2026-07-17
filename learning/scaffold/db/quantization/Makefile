# Makefile for 量化技术演示
CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -lm
TARGET = test
SRC = main.c

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET) *.o

run: $(TARGET)
	./$(TARGET)

.PHONY: clean run
