# =============================================================================
# logging 卡的 Makefile
# 运行: make (默认运行演示)
# =============================================================================

PYTHON := python3
SCRIPT := main.py

.PHONY: help run test clean

help:
	@echo "logging 演示 - Python 日志系统"
	@echo ""
	@echo "可用目标:"
	@echo "  make run   - 运行 logging 演示脚本"
	@echo "  make test  - 检查 Python3 可用性"
	@echo "  make clean - 清理演示产生的日志文件"
	@echo "  make help  - 显示本帮助信息"

run:
	@$(PYTHON) $(SCRIPT)

test:
	@echo "检查 Python3..."
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 已安装" || (echo "✗ Python3 未安装" && exit 1)
	@$(PYTHON) -c "import logging; print('✓ logging 模块可用')"

clean:
	@echo "清理日志文件..."
	@rm -rf /tmp/python_logging_demo 2>/dev/null || true
	@echo "✓ 清理完成"
