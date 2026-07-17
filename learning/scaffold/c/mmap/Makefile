CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = mmap_demo
SRC     = main.c

.PHONY: all run clean test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	@if [ -f /etc/passwd ]; then ./$(TARGET) /etc/passwd; \
	elif [ -f ../../../../etc/passwd ]; then ./$(TARGET) ../../../../etc/passwd; \
	else head -c 4096 /dev/urandom > /tmp/mmap_demo.bin && ./$(TARGET) /tmp/mmap_demo.bin; fi

test: $(TARGET)
	head -c 4096 /dev/urandom > /tmp/mmap_demo.bin
	./$(TARGET) /tmp/mmap_demo.bin

clean:
	rm -f $(TARGET) /tmp/mmap_demo.bin
