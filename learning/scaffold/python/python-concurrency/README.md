# python-concurrency scaffold

Python 并发编程综合演示——threading + multiprocessing + asyncio。

## 复现命令

```bash
cd learning/scaffold/python/python-concurrency
python3 main.py
```

## 关键点

- **threading**：I/O 密集型任务，受 GIL 限制
- **multiprocessing**：CPU 密集型任务，真并行
- **asyncio**：高并发 I/O，单线程协程

详见 NOTES.md。
