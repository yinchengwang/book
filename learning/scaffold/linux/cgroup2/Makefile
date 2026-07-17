# Makefile — cgroup2

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = cgroup2
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
