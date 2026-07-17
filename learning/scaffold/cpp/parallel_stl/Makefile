# parallel_stl/Makefile
# C++17 Parallel STL 执行策略演示
#
# C++17 标准库 execution policy，无需额外依赖
# Linux/macOS: 已包含在 libstdc++/ libc++
# Windows: MinGW g++ 14+ 支持

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -pthread
LDFLAGS =

TARGET = parallel_stl
SRC = main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
