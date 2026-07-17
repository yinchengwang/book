CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -g
TARGET = test
SRC = main.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
