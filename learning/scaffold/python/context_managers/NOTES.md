# context_managers 学习笔记

## 概念地图

Python 上下文管理器是资源管理的标准方式：

- **`__enter__`/`__exit__`**：协议方法，`with` 语句自动调用
- **`@contextmanager`**：用生成器实现上下文管理器，更简洁
- **资源获取即初始化**：RAII 的 Python 实现
- **异常处理**：`__exit__` 返回 `True` 抑制异常，否则传播
- **标准库工具**：`redirect_stdout`、`suppress`、`closing`

## 踩坑记录

1. **`__exit__` 返回值**：返回 `True` 会抑制异常，可能隐藏错误
2. **`yield` 后的代码**：无论是否异常都会执行（`try/finally` 语义）
3. **嵌套 with**：Python 3.10+ 支持 `with (A() as a, B() as b):`
4. **生成器状态**：yield 前后分别对应 enter/exit
5. **线程安全**：`threading.Lock` 支持 `with` 语句，自动释放

## 工程对照（≥100 字硬约束）

Python 上下文管理器在 `engineering/` 和标准库中有大量实践：

1. **文件操作**：`with open()` 自动关闭文件，防止资源泄漏
2. **数据库连接**：`with conn:` 自动提交或回滚事务
3. **线程锁**：`with lock:` 自动获取和释放锁，防止死锁
4. **`@contextmanager` 实现装饰器**：`@lru_cache` 可用上下文管理器管理缓存清理
5. **标准库应用**：`redirect_stdout()` 重定向打印输出
6. **资源池**：连接池、线程池用上下文管理器获取和归还资源

学完本卡能动手的事：实现一个 `@retry` 装饰器，支持最大重试次数和重试间隔。
