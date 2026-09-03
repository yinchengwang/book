# Makefile for C++ Memory Model Demo
# 使用 g++ -std=c++17 编译

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

TARGET = memory_model_demo
SRC = main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
