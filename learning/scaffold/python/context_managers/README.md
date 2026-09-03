# context_managers scaffold

Python 上下文管理器演示——with 语句 + __enter__/__exit__ + contextlib。

## 复现命令

```bash
cd learning/scaffold/python/context_managers
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[with_basic] === with 语句基本用法 ===
  文件已写入
  锁已获取，正在访问共享资源

[enter_exit] === __enter__/__exit__ 协议 ===
  [计算任务] 开始
  [计算任务] 结束，耗时 0.0004s

[contextlib] === contextlib 工具 ===
  <html>
  内容
  <body>
    页面内容
  </body>
  </html>

=== PASS ===
```

## 关键点

- **`with` 语句**：自动调用 `__enter__` 和 `__exit__`
- **`__enter__` 返回值**：绑定到 `as` 后的变量
- **`__exit__` 参数**：接收异常类型、值、追踪
- **`@contextmanager`**：用生成器简化上下文管理器实现
- **标准库工具**：`redirect_stdout`、`suppress`、`closing`

详见 NOTES.md 工程对照段。
