CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = redis_cluster

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
