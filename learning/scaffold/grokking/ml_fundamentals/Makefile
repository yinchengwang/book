.PHONY: all run clean

all:
	python -c "import py_compile; py_compile.compile('main.py', doraise=True)" && echo "语法检查通过"

run:
	python main.py

clean:
	rm -f __pycache__/*.pyc
	rmdir __pycache__ 2>/dev/null || true
