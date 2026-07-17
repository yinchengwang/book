# Makefile for C++ Move Semantics Demo
# 移动语义演示程序构建配置

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
TARGET = test
SRC = main.cpp

# 默认目标：构建可执行文件
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# 清理编译产物
clean:
	rm -f $(TARGET)

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 重新构建
rebuild: clean $(TARGET)

.PHONY: clean run rebuild
