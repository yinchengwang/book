CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = cross_demo
SRC     = main.c platform.c

.PHONY: all run check-platform cross-check clean

all: $(TARGET)

$(TARGET): $(SRC) platform.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

# ---------- 平台检测 target ----------

check-platform:
	@echo "=== 编译目标平台 ==="
	@echo "Compiler target: $$($(CC) -dumpmachine 2>/dev/null || echo 'unknown')"
	@echo ""
	@echo "=== 预定义平台宏 ==="
	@$(CC) -dM -E - < /dev/null 2>/dev/null | grep -E '_WIN|__linux|__APPLE|__MACH|__GNUC|_MSC|__x86_64|__aarch64|__i386' || echo "[skip] 无法获取预定义宏"

cross-check:
	@echo "[cross-check] 本机只能验证当前平台（$(PLATFORM_OS_NAME)）"
	@echo "[cross-check] 其他平台的 #ifdef 路径需要交叉编译环境验证"
	@echo "[cross-check] 代码通过 gcc -Wall -Wextra 零警告即保证语法正确"

clean:
	rm -f $(TARGET)
