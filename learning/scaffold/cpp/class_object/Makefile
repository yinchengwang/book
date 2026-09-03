CXX     ?= g++
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++17

TARGET  = class_demo
SRC     = main.cpp

.PHONY: all run clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)