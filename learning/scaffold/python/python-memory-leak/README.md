# python-memory-leak scaffold

Python 内存泄漏演示——泄漏场景 + 检测方法。

## 复现命令

```bash
cd learning/scaffold/python/python-memory-leak
python3 main.py
```

## 关键点

- **全局列表泄漏**：列表不断 append
- **闭包泄漏**：闭包持有大对象引用
- **缓存泄漏**：无限增长的缓存
- **tracemalloc**：追踪内存分配

详见 NOTES.md。
