CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = arrstr_demo
SRC     = main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)