# ReadView 机制演示 - Makefile
#
# 编译选项: gcc -std=c11 -Wall -o readview main.c

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = readview
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
