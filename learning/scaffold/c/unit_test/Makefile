CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET     = utest_demo
SRC_DEMO   = main.c mathlib.c

TARGET_TEST = test_runner
SRC_TEST    = test.c mathlib.c

.PHONY: all run test test-verbose coverage clean

all: $(TARGET)

$(TARGET): $(SRC_DEMO)
	$(CC) $(CFLAGS) -o $@ $^

run: $(TARGET)
	./$(TARGET)

# ---------- 测试 target ----------

$(TARGET_TEST): $(SRC_TEST)
	$(CC) $(CFLAGS) -o $@ $^

test: $(TARGET_TEST)
	./$(TARGET_TEST)

test-verbose: $(SRC_TEST)
	$(CC) $(CFLAGS) -DTEST_VERBOSE -o $(TARGET_TEST) $^
	./$(TARGET_TEST)

# ---------- 覆盖率（graceful 降级） ----------

coverage:
	@command -v gcov >/dev/null 2>&1 || { echo "[skip] gcov 不可用"; exit 0; }
	$(CC) $(CFLAGS) --coverage -o $(TARGET_TEST) $(SRC_TEST)
	./$(TARGET_TEST)
	@gcov mathlib.c 2>/dev/null && echo "[coverage] 查看 mathlib.c.gcov" || true

clean:
	rm -f $(TARGET) $(TARGET_TEST) *.o *.gcno *.gcda *.gcov
