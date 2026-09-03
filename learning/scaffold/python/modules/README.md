# modules scaffold

Python 模块与包演示——import/from/包管理 + __name__ + 模块搜索路径 + 导入别名。

## 复现命令

```bash
cd learning/scaffold/python/modules
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[import] === import 与 from import ===
  utils.add(10,20) -> 30
  multiply(3,4) -> 12

[package] === 包结构 ===
  utils.__file__ -> .../modules/utils.py
  utils.__name__ -> utils

[name] === __name__ 的作用 ===
  main.py __name__ -> __main__
  utils.__name__ -> utils

[alias] === 导入别名与再导出 ===
  statistics.mean([1..10]) -> 5.5

=== PASS ===
```

## 关键点

- **`import` vs `from import`**：前者保留命名空间，后者直接引入
- **`__name__`**：直接运行时是 `__main__`，被导入时是模块名
- **`__all__`**：定义 `from module import *` 时导出的符号
- **模块搜索路径**：sys.path 依次为脚本目录、PYTHONPATH、系统路径
- **单下划线 `_xxx`**：约定为私有，不被 `from module import *` 导入

详见 NOTES.md 工程对照段。
