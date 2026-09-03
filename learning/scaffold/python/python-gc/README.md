# python-gc scaffold

Python 垃圾回收演示——引用计数 + 循环 GC + gc 模块。

## 复现命令

```bash
cd learning/scaffold/python/python-gc
python3 main.py
```

## 关键点

- **引用计数**：Python 主要的 GC 机制
- **循环引用**：对象间相互引用导致引用计数不为0
- **gc.collect()**：手动触发垃圾回收
- **weakref**：不增加引用计数的引用

详见 NOTES.md。
