# =============================================================================
# ffi 卡的 Makefile
# =============================================================================

PYTHON := python3
SCRIPT := main.py

.PHONY: help run test clean

help:
	@echo "ffi 演示 - Python FFI（ctypes + cffi）"
	@echo "可用目标:"
	@echo "  make run   - 运行 FFI 演示"
	@echo "  make test  - 检查环境"
	@echo "  make clean - 清理"

run:
	@$(PYTHON) $(SCRIPT)

test:
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 可用" || exit 1
	@$(PYTHON) -c "import ctypes; print('✓ ctypes 可用')"

clean:
	@rm -rf /tmp/python_ffi_demo 2>/dev/null || true
	@echo "✓ 清理完成"
