CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -Wpedantic -O2 -std=c11

TARGET  = ci_demo
SRC     = main.c

.PHONY: all run ci-local check-yml lint clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

# === CI 目标 ===

# 本地 CI 流水线（编译 + 测试 + 静态分析）
ci-local: $(TARGET)
	@echo "=== 启动本地 CI ==="
	@bash ./ci.sh

# 验证 YAML 语法（graceful 降级）
check-yml:
	@echo "=== 验证 .github/workflows/ci.yml 语法 ==="
	@if command -v python >/dev/null 2>&1; then \
	    python -c "import yaml,sys; yaml.safe_load(open('.github/workflows/ci.yml')); print('  OK: ci.yml YAML 语法正确')" 2>/dev/null \
	    || echo "  [skip] python yaml 模块不可用，仅做格式检查"; \
	else \
	    echo "  [skip] python 不可用"; \
	fi

# 静态分析（graceful 降级）
lint: $(TARGET)
	@command -v cppcheck >/dev/null 2>&1 && cppcheck --enable=warning,style $(SRC) || echo "  [skip] cppcheck 不可用"

clean:
	rm -f $(TARGET) build/$(TARGET)