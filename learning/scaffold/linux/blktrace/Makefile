# Makefile — blktrace

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
LDFLAGS = -lpthread
TARGET = blktrace
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) /tmp/blktrace_test.dat
