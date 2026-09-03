# r31-python-advanced Specification

## Purpose
R31 Python 进阶学习卡的规格定义，确保 14 张 Python 进阶卡遵循统一的四要素审计标准。

## Requirements

### Requirement: Python 进阶卡四要素审计

每张 Python 进阶卡从 `todo` 推到 `done` 时，必须同时满足：

1. **scaffold 产物**：commit 包含至少 1 份可运行的 Python 源（`learning/scaffold/python/<card-id>/main.py` + `Makefile` + `README.md`）
2. **NOTES.md 工程对照**：路径为 `learning/scaffold/python/<card-id>/NOTES.md`，必须含 ≥100 中文字符的"工程对照"段落
3. **r31-progress.md 行**：与 `statuses.json` status 严格同步
4. **运行通过**：`python3 main.py` 执行无错误

### Requirement: 14 张 Python 进阶卡覆盖范围

| 卡 ID | 名称 | 主题 |
|-------|------|------|
| python-decorator | 装饰器 | @decorator + functools.wraps + 带参装饰器 + AOP |
| python-generator | 生成器 | yield + 生成器表达式 + itertools + 惰性计算 |
| python-context-manager | 上下文管理器 | contextlib + @contextmanager + RAII |
| python-metaclass | 元类 | type() + \_\_new\_\_ + ORM 示例 + 元编程 |
| python-closures | 闭包 | 闭包引用 + 工厂函数 + 延迟绑定 + 捕获陷阱 |
| python-asyncio | 异步编程 | async/await + asyncio + 并发任务 + 事件循环 |
| python-threading | 线程 | threading + 锁 + 条件变量 + 线程安全 |
| python-multiprocessing | 多进程 | multiprocessing + Pool + IPC + 进程安全 |
| python-type-hints | 类型提示 | TypeVar + Generic + Protocol + mypy |
| python-descriptors | 描述符 | \_\_get\_\_/\_\_set\_\_/\_\_delete\_\_ + property |
| python-slots | __slots__ | \_\_slots\_\_ + 内存优化 + 性能对比 |
| python-abc | 抽象基类 | ABC + abstractmethod + 接口设计 + 注册 |
| python-dataclasses | 数据类 | @dataclass + field + frozen + 继承 |
| python-advanced-summary | 进阶总结 | 14 张进阶卡全景回顾 + 进阶路线图 |

### Requirement: statuses.json 更新规范

当所有 14 张卡完成时：
- `statuses.json` 中 R31 相关条目的 done count 应从 269 增加到 283（计入 R30 基线）
- 具体更新的 key：`python-decorator`, `python-generator`, `python-context-manager`, `python-metaclass`, `python-closures`, `python-asyncio`, `python-threading`, `python-multiprocessing`, `python-type-hints`, `python-descriptors`, `python-slots`, `python-abc`, `python-dataclasses`, `python-advanced-summary`

### Requirement: r31-progress.md 格式规范

`learning/scaffold/python/r31-progress.md` 必须包含：
- 表头：`| 日期 | 卡 | 状态 |`
- 每张卡一行，日期为实际完成日期
- 总计行：`- 完成进度：14/14`
