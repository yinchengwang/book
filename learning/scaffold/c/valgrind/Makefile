CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O0 -g -std=c11

TARGET  = valgrind_demo
SRC     = main.c

.PHONY: all run valgrind clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

valgrind: $(TARGET)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes ./$(TARGET)

clean:
	rm -f $(TARGET)
