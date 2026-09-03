# 红黑树 Makefile
# 编译：gcc -std=c11 -Wall -O2 main.c -o rbtree

CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = rbtree
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
