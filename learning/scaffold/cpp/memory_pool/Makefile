# 内存池演示 Makefile
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

TARGET = memory_pool
SRC = main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
