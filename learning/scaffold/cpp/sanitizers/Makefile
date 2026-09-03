# Sanitizer 演示 Makefile
# 支持 ASan / MSan / TSan / USan 四种 Sanitizer 编译模式
#
# 使用方式:
#   make asan    - 编译并运行 AddressSanitizer 演示
#   make msan    - 编译并运行 MemorySanitizer 演示
#   make tsan    - 编译并运行 ThreadSanitizer 演示
#   makeusan     - 编译并运行 UndefinedBehaviorSanitizer 演示
#   make clean   - 清理编译产物
#   make all     - 编译全部 sanitizer 版本

CXX = g++
CXXFLAGS_COMMON = -std=c++17 -Wall -Wextra -g -O1
SRC = main.cpp
TARGET_BASE = sanitizer_demo

# 统一输出目录
BUILD_DIR = build
$(shell mkdir -p $(BUILD_DIR))

# ============================================================
# 1. AddressSanitizer (ASan) - 内存错误检测
# ============================================================
# 适用场景：heap/stack/global buffer overflow, use-after-free,
#          double-free, memory leak, return-stack-addr
# 检测能力：最强，覆盖所有常见内存错误
# 性能开销：~2x 内存，~2x CPU

ASAN_FLAGS = $(CXXFLAGS_COMMON) -fsanitize=address
ASAN_LDFLAGS = -fsanitize=address

$(BUILD_DIR)/$(TARGET_BASE)_asan: $(SRC)
	$(CXX) $(ASAN_FLAGS) -o $@ $< $(ASAN_LDFLAGS)

asan: $(BUILD_DIR)/$(TARGET_BASE)_asan
	./$(BUILD_DIR)/$(TARGET_BASE)_asan asan

# ============================================================
# 2. MemorySanitizer (MSan) - 未初始化内存检测
# ============================================================
# 适用场景：use of uninitialized memory
# 检测能力：检测所有未初始化内存读取（需要 LLVM/Clang）
# 性能开销：~3x 内存，~2-3x CPU
# 注意：MSan 仅支持 Clang，不支持 GCC

MSAN_FLAGS = $(CXXFLAGS_COMMON) -fsanitize=memory
MSAN_LDFLAGS = -fsanitize=memory

$(BUILD_DIR)/$(TARGET_BASE)_msan: $(SRC)
	$(CXX) $(MSAN_FLAGS) -o $@ $< $(MSAN_LDFLAGS)

msan: $(BUILD_DIR)/$(TARGET_BASE)_msan
	./$(BUILD_DIR)/$(TARGET_BASE)_msan msan

# ============================================================
# 3. ThreadSanitizer (TSan) - 数据竞争检测
# ============================================================
# 适用场景：data races in multithreaded programs
# 检测能力：检测所有数据竞争（两个线程同时访问同一内存，
#          至少一个写操作，无锁保护）
# 性能开销：~5-10x 内存，~2-5x CPU
# 注意：需要 -pthread 支持

TSAN_FLAGS = $(CXXFLAGS_COMMON) -fsanitize=thread -pthread
TSAN_LDFLAGS = -fsanitize=thread -pthread

$(BUILD_DIR)/$(TARGET_BASE)_tsan: $(SRC)
	$(CXX) $(TSAN_FLAGS) -o $@ $< $(TSAN_LDFLAGS)

tsan: $(BUILD_DIR)/$(TARGET_BASE)_tsan
	./$(BUILD_DIR)/$(TARGET_BASE)_tsan tsan

# ============================================================
# 4. UndefinedBehaviorSanitizer (USan) - 未定义行为检测
# ============================================================
# 适用场景：signed integer overflow, division by zero, null deref,
#          shift overflow, alignment violations
# 检测能力：覆盖 C++ 标准中所有未定义行为
# 性能开销：~1.5x 内存，~1.5x CPU

USAN_FLAGS = $(CXXFLAGS_COMMON) -fsanitize=undefined
USAN_LDFLAGS = -fsanitize=undefined

$(BUILD_DIR)/$(TARGET_BASE)_usan: $(SRC)
	$(CXX) $(USAN_FLAGS) -o $@ $< $(USAN_LDFLAGS)

usan: $(BUILD_DIR)/$(TARGET_BASE)_usan
	./$(BUILD_DIR)/$(TARGET_BASE)_usan usan

# ============================================================
# 5. 组合模式
# ============================================================

# ASan + USan 组合（常见组合，覆盖内存+未定义行为）
ASAN_USAN_FLAGS = $(CXXFLAGS_COMMON) -fsanitize=address,undefined
ASAN_USAN_LDFLAGS = -fsanitize=address,undefined

$(BUILD_DIR)/$(TARGET_BASE)_asan_usan: $(SRC)
	$(CXX) $(ASAN_USAN_FLAGS) -o $@ $< $(ASAN_USAN_LDFLAGS)

asan_usan: $(BUILD_DIR)/$(TARGET_BASE)_asan_usan
	./$(BUILD_DIR)/$(TARGET_BASE)_asan_usan asan

# ============================================================
# 6. 辅助目标
# ============================================================

# 编译全部版本
.PHONY: all
all: asan msan tsan usan

# 清理编译产物
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

# 显示帮助
.PHONY: help
help:
	@echo "Sanitizer 演示 Makefile"
	@echo ""
	@echo "可用目标:"
	@echo "  make asan        - AddressSanitizer (内存错误检测)"
	@echo "  make msan        - MemorySanitizer (未初始化内存检测)"
	@echo "  make tsan        - ThreadSanitizer (数据竞争检测)"
	@echo "  make usan        - UndefinedBehaviorSanitizer (未定义行为检测)"
	@echo "  make asan_usan   - ASan + USan 组合"
	@echo "  make all         - 编译全部版本"
	@echo "  make clean       - 清理编译产物"
	@echo ""
	@echo "编译产物位于 build/ 目录"

# ============================================================
# MSVC Sanitizer 支持说明
# ============================================================
# MSVC 原生不支持 ASan/MSan/TSan/USan。
# 如需在 Windows 上使用 Sanitizer：
#   选项 1: 使用 MinGW-w64 或 Clang-CL 编译器
#   选项 2: 使用 MSVC + AddressSanitizer (VS 2019+ 内置)
#           cl /fsanitize=address /Zi main.cpp
#           注意：MSVC 地址清理器功能有限
#
# 本 Makefile 设计为 Unix-like 环境（Linux/macOS）使用。
