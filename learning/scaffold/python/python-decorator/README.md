# python-decorator scaffold

Python 装饰器演示——@decorator + functools.wraps + 带参装饰器 + 类装饰器 + 装饰器栈。

## 复现命令

```bash
cd learning/scaffold/python/python-decorator
python3 main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[1] 基础装饰器:
[装饰器] 调用 greet 前
[装饰器] 调用 greet 后
    返回值: Hello, Alice!

[2] 带参数的装饰器:
    重复3次: ['Hello, Bob!', 'Hello, Bob!', 'Hello, Bob!']

[3] 计时装饰器:
[计时] slow_function 执行耗时: 0.000123s

[4] 参数验证装饰器:
    divide(10, 2) = 5 ✓
    divide(-5, 2) 抛出异常: 参数 0 必须为正数，实际为 -5 ✓

[5] 类装饰器:
    我的名字是 Charlie
    来自新方法的问候: Charlie

[6] 装饰器栈 (debug + memoize):
[调试] 调用 fibonacci(0)
[调试] fibonacci(0) 返回 0
    fibonacci(0) = 0
[记忆] 命中缓存: (1,)
    fibonacci(1) = 1
...

[7] functools.wraps 保留元信息:
    函数名: calculate
    文档: 计算 x + y

=== PASS ===
```

## 关键点

- **装饰器本质**：接收函数返回函数的闭包
- **@语法糖**：`@decorator` 等价于 `func = decorator(func)`
- **functools.wraps**：保留原函数的 `__name__`、`__doc__` 等元信息
- **带参装饰器**：需要三层嵌套（外层接收参数，内层是装饰器）
- **装饰器栈**：从下到上执行，先执行 memoize 再执行 debug

详见 NOTES.md 工程对照段。
