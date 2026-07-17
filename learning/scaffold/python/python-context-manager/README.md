# python-context-manager scaffold

Python 上下文管理器演示——with 语句 + @contextmanager + 资源管理。

## 复现命令

```bash
cd learning/scaffold/python/python-context-manager
python3 main.py
```

## 预期输出

```
[1] 类实现上下文管理器:
    [sleep] 耗时: 0.1004s

[2] @contextmanager 装饰器:
    [上下文] 耗时: 0.0502s
    [临时目录] 创建: /tmp/xxx
    写入文件: /tmp/xxx/test.txt
    [临时目录] 清理: /tmp/xxx

[3] ExitStack:
    --- ExitStack 演示 ---
    [资源 res0] 开启
    [资源 res1] 开启
    [资源 res2] 开启
    剩余资源: ['res0', 'res1', 'res2']

[4] 嵌套上下文:
    [外层] 耗时: 0.1004s
    [中层] 耗时: 0.0502s
    [内层] 耗时: 0.0501s

[5] 异常处理:
    --- 异常处理 ---
    捕获异常: division by zero
    异常被抑制，继续执行

[6] 数据库连接:
    [DB] 连接到 localhost:5432
    [DB] 执行: SELECT * FROM users
    [DB] 断开连接

=== PASS ===
```

## 关键点

- **__enter__/__exit__**：类实现上下文管理器的协议
- **@contextmanager**：用生成器函数简化上下文管理器实现
- **yield**：@contextmanager 中 yield 之前的代码在 __enter__ 执行，之后的在 __exit__ 执行
- **return True in __exit__**：抑制异常传播

详见 NOTES.md。
