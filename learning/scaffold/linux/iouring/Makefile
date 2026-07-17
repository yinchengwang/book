# Makefile — iouring

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = iouring
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) /tmp/iouring_test.dat
