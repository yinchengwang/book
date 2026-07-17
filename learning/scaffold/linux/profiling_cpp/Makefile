# profiling_cpp Makefile
# C++ 性能分析演示的编译脚本

CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -g
TARGET = profiling_demo

all: $(TARGET)

$(TARGET): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
	rm -f /tmp/profiling_io_demo.txt

perf:
	@echo "perf not available on Windows - use WSL"
	@echo "在 Linux (WSL) 下可使用以下命令："
	@echo "  perf top -p $$PID         # 实时观察 CPU 热点"
	@echo "  perf record -F 99 -g -p $$PID   # 采样（99Hz，带调用栈）"
	@echo "  perf report -g            # 查看采样报告"
	@echo "  perf stat -e cycles,instructions ./$(TARGET)  # 统计事件"

.PHONY: all run clean perf
