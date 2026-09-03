# debugging 学习笔记

## 概念地图

Python 调试工具体系从 print 到 pdb 到 IDE：

- **print**：最简单，适合快速定位
- **logging**：结构化，支持级别和输出控制
- **pdb**：交互式调试，断点/单步/变量检查
- **breakpoint()**：Python 3.7+ 内置断点
- **traceback**：异常堆栈追踪

## 踩坑记录

1. **print 在循环中**：大量输出影响性能
2. **logging 配置冲突**：多次 basicConfig 可能产生重复日志
3. **pdb 在 CI 中**：不要留下 `import pdb; pdb.set_trace()` 在生产代码中

## 工程对照（≥100 字硬约束）

### 1. 工程中的调试实践

本工程的 Python 脚本使用 logging 调试：

```python
# learning/scripts/sync-pipeline.py 中的调试模式
import logging
logging.basicConfig(level=logging.DEBUG)
logger = logging.getLogger(__name__)
logger.debug("正在同步文件：%s", filepath)
```

### 2. C 代码调试 vs Python 调试

| C 调试 | Python 调试 |
|--------|------------|
| gdb | pdb |
| fprintf(stderr, ...) | logging.warning() |
| assert() | assert |
| valgrind | tracemalloc |

### 3. 生产调试技巧

```python
# 通过环境变量控制调试级别
import os
logging.basicConfig(
    level=logging.DEBUG if os.getenv('DEBUG') else logging.INFO
)
```

学完本卡能动手的事：用 breakpoint() 和 pdb 命令高效排查 Python 代码问题。
