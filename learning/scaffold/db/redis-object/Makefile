CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = redis_object

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run
