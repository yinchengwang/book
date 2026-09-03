# =============================================================================
# distribution 卡的 Makefile
# =============================================================================

PYTHON := python3
SCRIPT := main.sh

.PHONY: help run test clean

help:
	@echo "distribution 演示 - Python 包分发"
	@echo "可用目标: make run, make test, make clean"

run:
	@chmod +x $(SCRIPT) 2>/dev/null || true
	@bash $(SCRIPT)

test:
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 可用" || exit 1

clean:
	@rm -rf /tmp/python_distribution_demo dist build *.egg-info 2>/dev/null || true
	@echo "✓ 清理完成"
