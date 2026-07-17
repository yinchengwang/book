# python-profiling scaffold

Python 性能分析演示——cProfile + timeit + tracemalloc。

## 复现命令

```bash
cd learning/scaffold/python/python-profiling
python3 main.py
```

## 关键点

- **timeit**：精确测量小代码片段
- **cProfile**：函数级性能分析
- **tracemalloc**：内存分配分析
- **sys.getsizeof**：对象大小估算

详见 NOTES.md。
