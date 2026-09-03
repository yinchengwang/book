CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = shortest_path_demo
SRC     = main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
