CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++20 -fcoroutines
TARGET = test
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
