# db_undo_log/Makefile
# Undo Log 学习演示构建配置

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra
TARGET = undo_log
SRC = main.c

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
