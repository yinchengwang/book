CC      ?= gcc
CXX     ?= g++
CFLAGS  ?= -Wall -Wextra -O2
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++17

TARGET  = cpp_cmake_demo
SRC     = main.cpp

# === CMake 构建 target ===
.PHONY: all run build cmake-clean clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -DAPP_VERSION='"1.0.0"' -DBUILD_TYPE='"direct-make"' -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

# 用 CMake 构建（创建独立 build 目录）
build:
	@echo "=== CMake 构建 ==="
	@command -v cmake >/dev/null 2>&1 || { echo "[skip] cmake 不可用"; exit 0; }
	@mkdir -p build
	@cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make
	@echo "=== 产物：build/cpp_cmake_demo ==="
	@cd build && ./cpp_cmake_demo

cmake-clean:
	@rm -rf build

clean:
	rm -f $(TARGET)