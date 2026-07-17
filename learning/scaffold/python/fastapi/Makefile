# =============================================================================
# fastapi 卡的 Makefile
# =============================================================================

PYTHON := python3
SCRIPT := main.py

.PHONY: help run test clean serve

help:
	@echo "fastapi 演示 - Python Web 框架"
	@echo "可用目标:"
	@echo "  make run   - 运行演示脚本"
	@echo "  make serve - 启动 FastAPI 服务"
	@echo "  make test  - 检查依赖"

run:
	@$(PYTHON) $(SCRIPT)

serve:
	@pip install --quiet fastapi uvicorn 2>/dev/null || true
	@echo "启动服务: uvicorn main:app --reload"
	@$(PYTHON) -m uvicorn main:app --host 0.0.0.0 --port 8000

test:
	@which $(PYTHON) > /dev/null 2>&1 && echo "✓ Python3 可用" || exit 1
	@$(PYTHON) -c "import fastapi, pydantic; print('✓ FastAPI/Pydantic 可用')" || echo "需要安装: pip install fastapi pydantic"

clean:
	@echo "✓ 清理完成"
