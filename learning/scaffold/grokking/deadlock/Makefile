CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -pthread
TARGET := main

.PHONY: run clean check

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)

run: $(TARGET)
	timeout 3 ./$(TARGET) || echo "(timeout expected if deadlock occurs)"

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o
