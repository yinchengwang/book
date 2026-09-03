# Makefile — mmap_db

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = mmap_db
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) /tmp/mmap_db_test.dat
