# python-asyncio scaffold

Python asyncio 异步编程演示——async/await + asyncio.gather + 信号量。

## 复现命令

```bash
cd learning/scaffold/python/python-asyncio
python3 main.py
```

## 预期输出

```
[1] 基础协程:
    [Alice] 开始...
    [Alice] 你好！

[2] asyncio.gather:
    [获取] https://api.example.com/users 开始...
    [获取] https://api.example.com/posts 开始...
    [获取] https://api.example.com/comments 开始...
    [获取] https://api.example.com/posts 完成
    [获取] https://api.example.com/users 完成
    [获取] https://api.example.com/comments 完成
    总耗时: 1.21s（并发优势）

[3] create_task:
    [Bob] 开始...
    主协程继续执行...
    ...
    任务结果: Hello from Bob

=== PASS ===
```

## 关键点

- **async def**：定义协程函数
- **await**：挂起等待另一个协程完成
- **asyncio.gather**：并发执行多个协程
- **asyncio.Semaphore**：控制最大并发数
- **async with**：异步上下文管理器

详见 NOTES.md。
