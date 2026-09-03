# =============================================================================
# pip_packages 卡的 Makefile
# 运行: make (默认运行演示)
# =============================================================================

PYTHON := python3
SCRIPT := main.sh
VENV := venv

.PHONY: help run test clean install-deps

help:
	@echo "pip_packages 演示 - Python pip 高级用法"
	@echo ""
	@echo "可用目标:"
	@echo "  make run         - 运行 pip 演示脚本"
	@echo "  make install-deps- 在虚拟环境中安装依赖并运行"
	@echo "  make test        - 检查 pip 可用性"
	@echo "  make clean       - 清理演示产生的临时文件"

run:
	@chmod +x $(SCRIPT) 2>/dev/null || true
	@bash $(SCRIPT)

install-deps:
	@$(PYTHON) -m venv $(VENV)
	@. $(VENV)/bin/activate && pip install --quiet requests click colorama && bash $(SCRIPT)
	@rm -rf $(VENV)

test:
	@echo "检查 pip..."
	@which pip > /dev/null 2>&1 && echo "✓ pip 已安装" || (echo "✗ pip 未安装" && exit 1)
	@$(PYTHON) -m pip --version

clean:
	@echo "清理临时文件..."
	@rm -rf /tmp/python_pip_demo $(VENV) 2>/dev/null || true
	@echo "✓ 清理完成"
