CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g
TARGET = test.exe

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET)