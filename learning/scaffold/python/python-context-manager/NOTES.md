# python-context-manager 学习笔记

## 概念地图

Python 上下文管理器实现资源获取/释放模式（RAII）：

- **__enter__**：with 块开始时调用，返回值绑定到 as 后的变量
- **__exit__**：with 块结束时调用，无论正常还是异常
- **@contextmanager**：用生成器 + yield 简化上下文管理器
- **ExitStack**：动态管理多个上下文管理器

## RAII Python 实现

Python 上下文管理器是 RAII（Resource Acquisition Is Initialization）模式的 Python 版：

| C++ | Python |
|-----|--------|
| 构造函数获取资源 | `__enter__` |
| 析构函数释放资源 | `__exit__` |
| 栈对象自动析构 | with 语句自动清理 |

## 踩坑记录

1. **异常传播**：默认 `__exit__` 返回 False 会传播异常
2. **finally 语义**：@contextmanager 中 yield 后的代码在 try/finally 中执行
3. **as 子句**：`with cm() as var` 中 var 是 `__enter__` 的返回值

## 工程对照（≥100 字硬约束）

上下文管理器在 `engineering/` 中有广泛实践：

1. **文件操作**：`with open(path) as f` 自动关闭文件，避免资源泄漏
2. **锁管理**：`with threading.Lock():` 自动释放锁，与 RAII 等价
3. **临时文件**：`tempfile.NamedTemporaryFile` 可作为上下文管理器
4. **数据库连接**：`with connection.cursor() as cur` 自动提交或回滚
5. **测试隔离**：`with pytest.raises(Exception)` 验证异常抛出
6. **@contextmanager**：在 `sync-pipeline.py` 中封装数据库事务或文件锁

学完本卡能动手的事：将 open() 调用全部改为 with 语句，确保文件正确关闭。
