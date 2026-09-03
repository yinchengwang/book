CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = graph
SRC = main.c
OBJ = main.o

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJ): $(SRC)
	$(CC) $(CFLAGS) -c -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
