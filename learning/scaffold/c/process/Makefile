CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11

TARGET  = process_demo
SRC     = main.c

.PHONY: all run check-pid clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC)

run: $(TARGET)
	./$(TARGET)

# 打印当前 shell 进程标识符（POSIX only）
check-pid:
	@echo "=== 当前 shell 进程标识符 ==="
	@echo "PID ($$)        = $$"
	@echo "PPID            = $(shell echo $$PPID)"
	@echo "PGID            = $(shell ps -o pgid= -p $$ 2>/dev/null || echo unknown)"
	@echo "SID (sessionid) = $(shell ps -o sess= -p $$ 2>/dev/null || echo unknown)"

clean:
	rm -f $(TARGET)