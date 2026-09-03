CC      ?= gcc
CFLAGS  ?= -Wall -Wextra -O2 -std=c11
SECURITY_CFLAGS = -Wformat=2 -Wformat-security -fstack-protector-strong -D_FORTIFY_SOURCE=2

TARGET  = secure_demo
SRC     = main.c

.PHONY: all run run-vuln check-security check-flawfinder clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SECURITY_CFLAGS) -o $@ $<

run: $(TARGET)
	./$(TARGET)

# 漏洞版本（-DVULN_DEMO）
run-vuln: $(SRC)
	$(CC) $(CFLAGS) -DVULN_DEMO -o $(TARGET)_vuln $<
	./$(TARGET)_vuln
	@rm -f $(TARGET)_vuln

# 编译期安全警告统计
check-security:
	@echo "=== 安全编译检查 ==="
	@echo "Warning count: $$( ($(CC) $(CFLAGS) $(SECURITY_CFLAGS) -c $(SRC) -o /dev/null 2>&1 || true) | grep -c warning )"
	@echo "[done] 查看上方 warning 详情"

check-flawfinder:
	@command -v flawfinder >/dev/null 2>&1 && \
		flawfinder $(SRC) || \
		echo "[skip] flawfinder 不可用"

clean:
	rm -f $(TARGET) $(TARGET)_vuln
