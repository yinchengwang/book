# Makefile — ebpf_intro

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = ebpf_intro
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) /tmp/ebpf_test.log
