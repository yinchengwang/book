CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = two_pointers
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
