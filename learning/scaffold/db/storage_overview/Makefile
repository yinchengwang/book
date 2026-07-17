CC = gcc
CFLAGS = -std=c11 -Wall
TARGET = test

all: $(TARGET)
$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<
clean:
	rm -f $(TARGET)
