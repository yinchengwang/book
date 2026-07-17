CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

TARGET = main
SRC = main.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

# 运行
run: $(TARGET)
	./$(TARGET)

# clang-tidy 检查（需要安装 clang-tidy）
tidy:
	@if command -v clang-tidy >/dev/null 2>&1; then \
		clang-tidy $(SRC) -- -std=c++17 $(CXXFLAGS); \
	else \
		echo "clang-tidy not installed"; \
	fi

# 自动修复（谨慎使用）
fix:
	@if command -v clang-tidy >/dev/null 2>&1; then \
		clang-tidy $(SRC) -fix -- -std=c++17 $(CXXFLAGS); \
	else \
		echo "clang-tidy not installed"; \
	fi

clean:
	rm -f $(TARGET)

.PHONY: all clean run tidy fix
