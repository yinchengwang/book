# ARIES 学习演示 Makefile
#
# 使用 gcc 编译，标准 C11，开启所有警告。

CC      := gcc
CFLAGS  := -std=c11 -Wall -Wextra -pedantic
TARGET  := aries
SRC     := main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
