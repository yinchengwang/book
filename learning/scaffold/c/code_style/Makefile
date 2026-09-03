# c-code-style scaffold — Makefile 驱动 clang-format 检查

CC       ?= gcc
CFLAGS   ?= -Wall -Wextra -O2 -std=c11

CLANG_FORMAT ?= clang-format
STYLE_FILE   ?= ../../../../.clang-format
DEMO_SRC     := demo.c

.PHONY: all clean check fix

all: check

# clang-format 干检查：有差异 → exit 1
check:
	@if ! command -v $(CLANG_FORMAT) >/dev/null 2>&1; then \
	    echo "[style] clang-format 不可用，跳过检查"; \
	    exit 0; \
	fi
	@echo "[style] 已安装 clang-format，正在检查 ..."
	$(CLANG_FORMAT) --dry-run --Werror $(DEMO_SRC)

# 自动修复
fix:
	$(CLANG_FORMAT) -i $(DEMO_SRC)

clean:
	rm -f *.clang-format-diff
