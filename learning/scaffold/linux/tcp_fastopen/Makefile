# Makefile for TCP Fast Open 演示程序
#
# 编译: make
# 运行: sudo make run  (需要 root 权限修改 sysctl)
# 清理: make clean

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
TARGET = tcp_fastopen
SRC = main.c

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)
	@echo "编译完成: $(TARGET)"

run: $(TARGET)
	@echo "运行 TFO 演示程序 (需要 root 权限)..."
	@echo ""
	sudo ./$(TARGET)

clean:
	rm -f $(TARGET)
	@echo "清理完成"
