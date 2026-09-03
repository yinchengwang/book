CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = preproc_demo
SRC     = main.c

.PHONY: all run dump-macros clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

# 打印本机所有 GCC 预定义宏（仅前 20 行）
dump-macros:
	@echo "=== GCC 预定义宏（前 20 行） ==="
	@$(CC) -E -dM - < /dev/null 2>/dev/null | head -20 || echo "[skip] 无法获取预定义宏"

clean:
	rm -f $(TARGET)