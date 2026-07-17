CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = bit_manipulation
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
