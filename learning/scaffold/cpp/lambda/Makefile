# Makefile for Lambda Expression Demo
# ======================================

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
TARGET = test
SRC = main.cpp

# 默认目标
all: $(TARGET)

# 编译链接
$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# 运行
run: $(TARGET)
	./$(TARGET)

# 清理
clean:
	rm -f $(TARGET)

# 伪目标
.PHONY: all clean run
