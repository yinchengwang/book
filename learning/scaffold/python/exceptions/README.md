# exceptions scaffold

Python 异常处理演示——try/except/finally + 自定义异常 + 异常链。

## 复现命令

```bash
cd learning/scaffold/python/exceptions
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[try_except] === try/except 基本用法 ===
  捕获 ZeroDivisionError: 除数不能为零

[multi] === 多异常捕获 ===
  divide_safe(10, 2) -> 5.0
  divide_safe(10, 0) -> None

[raise] === raise 抛出异常 ===
  validate_age(-5) 抛出: 年龄不能为负数
  validate_age(25) -> 25

[chain] === 异常链与自定义异常 ===
  自定义异常: user: 用户名至少3个字符; 邮箱格式无效
    field: user, message: ...

=== PASS ===
```

## 关键点

- **`try/except`**：捕获异常，防止程序崩溃
- **`else`**：只有没有异常时执行
- **`finally`**：无论是否有异常都执行，用于资源清理
- **自定义异常**：继承 `Exception`，添加业务特定信息
- **异常链**：`raise ... from e` 保留原始异常上下文

详见 NOTES.md 工程对照段。
