# python-decorator 学习笔记

## 概念地图

Python 装饰器是 AOP（面向切面编程）的实现方式：

- **核心原理**：装饰器是高阶函数，`@decorator` 语法糖等价于 `func = decorator(func)`
- **闭包**：装饰器内部定义的 `wrapper` 函数捕获外部作用域的 `func`，形成闭包
- **functools.wraps**：调用 `functools.wraps(func)` 装饰 wrapper，保留原函数元信息
- **带参装饰器**：三层嵌套结构 `def repeat(times): def decorator(func): def wrapper(...)`
- **类装饰器**：装饰器也可以是类，只要实现了 `__call__` 方法

## 装饰器应用场景

1. **日志记录**：`@log_calls` 自动记录函数调用参数和返回值
2. **性能计时**：`@timer` 测量函数执行时间
3. **缓存**：`@memoize` 记忆化递归函数
4. **权限检查**：`@requires_auth` 检查用户权限
5. **参数验证**：`@validate` 运行时类型检查
6. **重试机制**：`@retry(max_attempts=3)` 网络请求重试

## 踩坑记录

1. **元信息丢失**：不使用 `functools.wraps` 会导致 `func.__name__` 变成 `wrapper`
2. **类装饰器方法绑定**：装饰器添加的方法不会自动 `self` 绑定
3. **装饰器顺序**：`@debug` 在 `@memoize` 上方时，先执行 memoize 再 debug
4. **带参数装饰器返回值**：装饰器工厂必须返回装饰器函数

## 工程对照（≥100 字硬约束）

AOP 编程在 `engineering/` 中有实际应用：

1. **日志装饰器**：`sync-pipeline.py` 可用 `@timer` 装饰批量文件处理函数，监控性能
2. **重试装饰器**：调用外部 API（GitHub REST）时，`@retry(max_attempts=3)` 处理网络抖动
3. **缓存装饰器**：`@memoize` 可用于缓存 `git diff --stat` 的结果，避免重复计算
4. **参数验证装饰器**：`validate_positive` 模式可用于验证文件路径、端口号等
5. **类装饰器**：Django/Flask 等 Web 框架用类装饰器添加路由和中间件
6. **装饰器栈**：组合多个简单装饰器实现复杂功能，如 `@auth @cache @rate_limit`

学完本卡能动手的事：为自己的 Python 工具编写日志计时装饰器，分析脚本瓶颈。
