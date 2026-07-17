CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = prof_demo
SRC     = main.c

.PHONY: all run profile perf valgrind-cachegrind clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

# ---------- Profiling target ----------

profile:
	@echo "=== gprof profiling ==="
	@command -v gprof >/dev/null 2>&1 || { echo "[skip] gprof 不可用（MinGW 通常不包含）"; exit 0; }
	$(CC) -pg $(CFLAGS) -o $(TARGET)_pg $(SRC)
	./$(TARGET)_pg
	gprof $(TARGET)_pg gmon.out
	@rm -f $(TARGET)_pg gmon.out

perf:
	@command -v perf >/dev/null 2>&1 || { echo "[skip] perf 不可用（Linux only）"; exit 0; }
	@echo "[perf] sudo perf stat ./$(TARGET)"
	@sudo perf stat ./$(TARGET) 2>/dev/null || echo "[skip] 权限不足或无 perf 支持"

valgrind-cachegrind:
	@command -v valgrind >/dev/null 2>&1 || { echo "[skip] valgrind 不可用（Linux only）"; exit 0; }
	valgrind --tool=cachegrind ./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET)_pg gmon.out cachegrind.out.*
