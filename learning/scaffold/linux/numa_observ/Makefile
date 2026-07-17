# Makefile — numa_observ

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
LDFLAGS = -lnuma
TARGET = numa_observ
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
