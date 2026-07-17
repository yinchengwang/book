# Makefile for C++ Template Metaprogramming Demo

# 编译器与编译选项
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g

# 目标文件
TARGET = test
SRC = main.cpp

# 默认目标
all: $(TARGET)

# 编译规则
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# 运行目标
run: $(TARGET)
	./$(TARGET)

# 清理目标
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all clean run
