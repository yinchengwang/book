# exceptions 学习笔记

## 概念地图

Python 异常处理机制是错误恢复的基础：

- **EAFP (Easier to Ask Forgiveness than Permission)**：先尝试，失败再处理——Pythonic 的错误处理风格
- **异常层次**：所有异常的根是 `BaseException`，常用异常都继承自 `Exception`
- **异常捕获顺序**：先捕获具体异常，后捕获通用异常
- **`raise`**：主动抛出异常，可带自定义消息
- **自定义异常**：业务错误码的 Pythonic 表达方式
- **异常链**：`__cause__` 保留原始异常，`__context__` 自动设置

## 踩坑记录

1. **裸 `except`**：捕获所有异常包括 `KeyboardInterrupt`，用 `except Exception`
2. **异常掩盖**：`except` 中再次抛出异常会丢失原异常链，用 `raise ... from e`
3. **资源泄漏**：忘记 `close()` 文件/连接，用 `with` 语句自动管理
4. **异常捕获过于宽泛**：捕获 `Exception` 而非具体异常，难以定位问题
5. **finally 中的 return**：`finally` 的 return 会覆盖 `try` 中的 return

## 工程对照（≥100 字硬约束）

Python 异常处理在 `learning/scripts/sync-pipeline.py` 和 `engineering/` 中有大量实践：

1. **异常作为控制流**：`sync-pipeline.py` 中 `try: int(value)` 捕获 `ValueError` 判断是否为数字
2. **自定义业务异常**：Web 框架（Flask/FastAPI）定义 `ValidationError`、`AuthenticationError` 等
3. **`finally` 资源清理**：`with open()` 底层使用 `try/finally`，确保文件关闭
4. **异常链保留**：`logging.exception()` 自动记录 `__context__`，便于调试
5. **异常安全**：C++ 的 `noexcept` 与 Python 的 `raise` 对应，保证不抛出异常的函数标记
6. **上下文管理器**：`with` 语句是 `try/finally` 的语法糖，资源管理更安全

学完本卡能动手的事：实现一个 `retry(attempts)` 装饰器，捕获异常时自动重试。
