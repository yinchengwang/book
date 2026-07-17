# Makefile — latency_diag

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = latency_diag
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
