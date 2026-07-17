CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = so_reuseport

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c -lpthread

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: run clean
