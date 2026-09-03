# Makefile — pagecache_hit

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = pagecache_hit
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) /tmp/pagecache_test.dat
