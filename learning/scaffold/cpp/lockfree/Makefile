# 无锁数据结构示例

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -lpthread

TARGET = lockfree
SRC = main.cpp

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
