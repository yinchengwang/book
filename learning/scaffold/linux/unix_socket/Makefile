# Unix Domain Socket 示例 Makefile
# 编译: make
# 运行: make run
# 清理: make clean

CC = gcc
CFLAGS = -std=c11 -Wall -O2
TARGET = unix_socket

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
