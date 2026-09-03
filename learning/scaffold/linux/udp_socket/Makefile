# UDP Socket 高性能示例 Makefile
# C11 标准，Linux 专用

CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
CFLAGS += -D_GNU_SOURCE  # 启用 GNU 扩展

TARGET = udp_socket
SRC = main.c

.PHONY: all run clean

# 默认目标：构建可执行文件
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<
	@echo "构建完成: $(TARGET)"

# 运行服务器（后台）
run: $(TARGET)
	@echo "启动 UDP 服务器 (端口 8888)..."
	@echo "  服务器模式: ./$(TARGET) server 8888"
	@echo "  客户端模式: ./$(TARGET) client 127.0.0.1 8888 10"
	@echo "  查看统计: ./$(TARGET) stats"
	./$(TARGET) server 8888

# 清理编译产物
clean:
	rm -f $(TARGET)
	@echo "清理完成"

# 调试构建
debug: CFLAGS += -DDEBUG -O0
debug: $(TARGET)

# 发布构建（去除调试符号）
release: CFLAGS = -std=c11 -Wall -O3 -DNDEBUG
release: $(TARGET)
	strip -s $(TARGET)
	@echo "发布版本构建完成: $(TARGET)"
