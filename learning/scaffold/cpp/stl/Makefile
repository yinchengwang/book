# Mystl Makefile
#
# 使用方法：
#   make           # 编译演示程序
#   make demo      # 编译演示程序
#   make test      # 编译所有测试
#   make run       # 运行演示程序
#   make clean     # 清理

CXX     ?= g++
CXXFLAGS ?= -Wall -Wextra -O2 -std=c++17
INC_DIR  = include
TEST_DIR = test

# 演示程序
DEMO_TARGET = stl_demo
DEMO_SRC    = main.cpp

# 测试文件
TESTS = $(wildcard $(TEST_DIR)/*_test.cpp)

.PHONY: all demo test run clean clean-test

# 默认：编译演示程序
all: demo

demo: $(DEMO_TARGET)

$(DEMO_TARGET): $(DEMO_SRC)
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -o $@ $(DEMO_SRC)

run: $(DEMO_TARGET)
	./$(DEMO_TARGET)

# 测试
test: $(TESTS:.cpp=)
	@echo "编译完成，运行: make run-test"

# 通用测试规则
$(TEST_DIR)/%_test: $(TEST_DIR)/%_test.cpp
	$(CXX) $(CXXFLAGS) -I$(INC_DIR) -o $@ $<

clean:
	rm -f $(DEMO_TARGET)

clean-test:
	rm -f $(wildcard $(TEST_DIR)/*_test)
