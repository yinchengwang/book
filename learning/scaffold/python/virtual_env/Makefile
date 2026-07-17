# =============================================================================
# virtual_env 卡的 Makefile
# 运行: make (默认运行演示)
# =============================================================================

PYTHON := python3
SCRIPT := main.sh

.PHONY: help run test clean

help:
	@echo "virtual_env 演示 - Python 虚拟环境"
	@echo ""
	@echo "可用目标:"
	@echo "  make run    - 运行 venv/virtualenv 演示脚本"
	@echo "  make test   - 检查 Python3 和 venv 模块可用性"
	@echo "  make clean  - 清理演示产生的临时文件"
	@echo "  make help   - 显示本帮助信息"

run:
	@chmod +x $(SCRIPT) 2>/dev/null || true
	@bash $(SCRIPT)

test:
	@echo "检查 Python3..."
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 已安装" || (echo "✗ Python3 未安装" && exit 1)
	@echo "检查 venv 模块..."
	@$(PYTHON) -c "import venv" 2>/dev/null && echo "✓ venv 模块可用" || echo "✗ venv 模块不可用"

clean:
	@echo "清理临时文件..."
	@rm -rf /tmp/python_venv_demo 2>/dev/null || true
	@echo "✓ 清理完成"
