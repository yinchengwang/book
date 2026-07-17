# Makefile — io_uring_storage

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = io_uring_storage
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) /tmp/io_uring_storage_test.dat
