# =============================================================================
# debugging 卡的 Makefile
# =============================================================================

PYTHON := python3
SCRIPT := main.py

.PHONY: help run test clean

help:
	@echo "debugging 演示 - Python 调试技巧"
	@echo "可用目标:"
	@echo "  make run   - 运行调试演示"
	@echo "  make pdb   - 使用 pdb 交互式运行"
	@echo "  make test  - 检查环境"

run:
	@$(PYTHON) $(SCRIPT)

pdb:
	@$(PYTHON) -m pdb $(SCRIPT)

test:
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 可用" || exit 1

clean:
	@echo "✓ 清理完成"
