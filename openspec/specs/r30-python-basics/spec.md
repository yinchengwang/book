# r30-python-basics Specification

## Purpose
R30 Python 基础学习卡的规格定义，确保 9 张 Python 基础卡遵循统一的四要素审计标准。

## Requirements

### Requirement: Python 基础卡四要素审计

每张 Python 基础卡从 `todo` 推到 `done` 时，必须同时满足：

1. **scaffold 产物**：commit 包含至少 1 份可运行的 Python 源（`learning/scaffold/python/<card-id>/main.py` + `Makefile` + `README.md`）
2. **NOTES.md 工程对照**：路径为 `learning/scaffold/python/<card-id>/NOTES.md`，必须含 ≥100 中文字符的"工程对照"段落
3. **r30-progress.md 行**：与 `statuses.json` status 严格同步
4. **运行通过**：`python3 main.py` 执行无错误

### Requirement: 9 张 Python 基础卡覆盖范围

| 卡 ID | 名称 | 主题 |
|-------|------|------|
| basic_types | 基础类型 | int/float/str/bool/list/dict/set + 类型转换 |
| functions | 函数 | def/lambda + 默认参数 + *args/**kwargs + 闭包 |
| classes | 类 | class/__init__/继承 + 多态 + 特殊方法 |
| modules | 模块 | import/from/包管理 + __name__ |
| exceptions | 异常处理 | try/except/finally + 自定义异常 + 异常链 |
| file_io | 文件 I/O | open/read/write + with 语句 + JSON/CSV |
| db_access | 数据库访问 | sqlite3 + 增删改查 + 事务 |
| comprehensions | 推导式 | list/dict/set 生成式 + 生成器表达式 |
| context_managers | 上下文管理器 | with 语句 + __enter__/__exit__ + contextlib |

### Requirement: statuses.json 更新规范

当所有 9 张卡完成时：
- `statuses.json` 中 R30 相关条目的 done count 应从 47 增加到 56
- 具体更新的 key：`basic_types`, `functions`, `classes`, `modules`, `exceptions`, `file_io`, `db_access`, `comprehensions`, `context_managers`

