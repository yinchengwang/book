# =============================================================================
# numpy 卡的 Makefile
# =============================================================================

PYTHON := python3
SCRIPT := main.py

.PHONY: help run test clean

help:
	@echo "numpy 演示 - NumPy 科学计算"
	@echo "可用目标:"
	@echo "  make run   - 运行 NumPy 演示"
	@echo "  make test  - 检查 numpy 安装"
	@echo "  make clean - 清理"

run:
	@$(PYTHON) $(SCRIPT)

test:
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 可用" || exit 1
	@$(PYTHON) -c "import numpy; print(f'✓ NumPy {numpy.__version__} 可用')"

clean:
	@echo "✓ 清理完成"
