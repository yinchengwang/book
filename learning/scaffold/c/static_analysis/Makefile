CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = static_analysis_demo
SRC     = main.c

.PHONY: all run check check-cppcheck check-flawfinder check-all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

# ---------- 静态分析 target ----------

check:
	@echo "=== GCC 静态检查 ==="
	@$(CC) -Wall -Wextra -Wpedantic -Wconversion -std=c11 -c $(SRC) -o /dev/null 2>&1; true
	@echo "[done] 查看上方 warning 输出"

check-cppcheck:
	@command -v cppcheck >/dev/null 2>&1 && \
		cppcheck --enable=all --inconclusive $(SRC) || \
		echo "[skip] cppcheck 不可用"

check-flawfinder:
	@command -v flawfinder >/dev/null 2>&1 && \
		flawfinder $(SRC) || \
		echo "[skip] flawfinder 不可用"

check-all: check check-cppcheck check-flawfinder
	@echo "[done] 所有静态检查完成"

clean:
	rm -f $(TARGET)
