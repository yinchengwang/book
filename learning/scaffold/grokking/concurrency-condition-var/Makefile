CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -pthread
TARGET = concurrency-condition-var
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
