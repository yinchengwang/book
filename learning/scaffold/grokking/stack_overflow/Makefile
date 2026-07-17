CC := gcc
CFLAGS := -std=c11 -Wall -Wextra -pthread
TARGET := main
.PHONY: run clean check

$(TARGET): main.c
	$(CC) $(CFLAGS) main.c -o $(TARGET)
run: $(TARGET)
	./$(TARGET)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) *.o
