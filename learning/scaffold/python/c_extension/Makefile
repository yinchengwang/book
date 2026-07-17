# =============================================================================
# c_extension 卡的 Makefile
# =============================================================================

PYTHON := python3
SCRIPT := main.sh

.PHONY: help run test clean

help:
	@echo "c_extension 演示 - Python C 扩展"
	@echo "可用目标:"
	@echo "  make run   - 编译并测试 C 扩展"
	@echo "  make test  - 检查编译工具"
	@echo "  make clean - 清理编译产物"

run:
	@chmod +x $(SCRIPT)
	@bash $(SCRIPT)

test:
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 可用" || exit 1
	@$(PYTHON) -c "from setuptools import setup, Extension; print('✓ setuptools 可用')"

clean:
	@rm -rf build *.so *.egg-info /tmp/python_cext_demo 2>/dev/null || true
	@echo "✓ 清理完成"
