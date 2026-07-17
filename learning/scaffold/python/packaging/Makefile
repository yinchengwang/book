# =============================================================================
# packaging 卡的 Makefile
# =============================================================================

PYTHON := python3
SCRIPT := main.sh

.PHONY: help run test clean build

help:
	@echo "packaging 演示 - Python 包打包分发"
	@echo "可用目标: make run, make build, make test, make clean"

run:
	@chmod +x $(SCRIPT) 2>/dev/null || true
	@bash $(SCRIPT)

test:
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 可用" || exit 1
	@$(PYTHON) -c "import setuptools; print('✓ setuptools 可用')"

build:
	@pip install --quiet build 2>/dev/null || true
	@cd /tmp/python_packaging_demo/mypackage && python3 -m build 2>&1 || echo "跳过构建"

clean:
	@rm -rf /tmp/python_packaging_demo dist build *.egg-info 2>/dev/null || true
	@echo "✓ 清理完成"
