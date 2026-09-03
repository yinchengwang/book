# Graph Traversal Scaffold Makefile
# 使用 gcc -std=c11 编译

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = graph_traversal
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
