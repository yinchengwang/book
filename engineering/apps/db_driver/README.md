# Build My DB Python 驱动

类似 psycopg2 的 Python 数据库驱动，用于连接 Build My DB 数据库。

## 目录结构

```
engineering/apps/db_driver/
├── __init__.py          # 主入口，导出 connect() 和异常类
├── connection.py        # Connection 类
├── cursor.py            # Cursor 类
├── cli.py               # CLI 执行器（通过 subprocess 调用 db_cli）
├── exceptions.py        # 异常类定义
├── test_driver.py       # 单元测试
├── examples.py          # 使用示例
└── README.md            # 本文档
```

## 快速开始

### 1. 构建 CLI 工具

```bash
cd c:/code/book
cmake --build build --target db_cli
```

### 2. 基本使用

```python
import build_my_db as db

# 连接数据库
conn = db.connect("my.db")

# 创建游标
cursor = conn.cursor()

# 创建表
cursor.execute("""
    CREATE TABLE documents (
        id INT PRIMARY KEY,
        title TEXT,
        content TEXT
    )
""")

# 插入数据
cursor.execute(
    "INSERT INTO documents VALUES (%s, %s, %s)",
    (1, "Python 教程", "Python 是一种高级编程语言...")
)

# 查询数据
cursor.execute("SELECT * FROM documents WHERE id = %s", (1,))
row = cursor.fetchone()
print(row)  # (1, 'Python 教程', 'Python 是一种高级编程语言...')

# 提交事务
conn.commit()

# 关闭连接
conn.close()
```

### 3. 使用上下文管理器（推荐）

```python
import build_my_db as db

with db.connect("my.db") as conn:
    cursor = conn.cursor()
    cursor.execute("CREATE TABLE users (id INT, name TEXT)")
    cursor.execute("INSERT INTO users VALUES (%s, %s)", (1, "Alice"))
    # 自动提交，自动关闭
```

## API 参考

### connect(database, **kwargs)

创建数据库连接。

| 参数 | 类型 | 说明 |
|-----|------|------|
| database | str | 数据库文件路径 |
| host | str | 主机地址（未来支持，默认 localhost） |
| port | int | 端口号（未来支持，默认 5432） |
| timeout | int | 超时时间（秒，默认 30） |
| cli_path | str | CLI 工具路径 |

返回: `Connection` 对象

### Connection 类

| 方法 | 说明 |
|-----|------|
| cursor() | 创建游标 |
| commit() | 提交事务 |
| rollback() | 回滚事务 |
| close() | 关闭连接 |
| execute(sql, params) | 便捷方法：执行 SQL 并返回游标 |
| query(sql, params) | 便捷方法：执行 SQL 并返回所有行 |

### Cursor 类

| 方法 | 说明 |
|-----|------|
| execute(sql, params) | 执行单条 SQL |
| executemany(sql, params_list) | 执行多条 SQL |
| fetchone() | 获取下一行 |
| fetchmany(size) | 获取多行 |
| fetchall() | 获取所有行 |
| close() | 关闭游标 |

| 属性 | 说明 |
|-----|------|
| description | 列信息 |
| rowcount | 影响的行数 |
| arraysize | fetchmany 默认行数 |

## 异常类

```python
from build_my_db import (
    Error,                  # 基类
    DatabaseError,          # 数据库错误
    OperationalError,       # 操作错误（如连接断开）
    ProgrammingError,       # 编程错误（如 SQL 语法错误）
    IntegrityError,         # 完整性约束错误（如主键冲突）
)
```

## 连接池

```python
import build_my_db as db

# 创建连接池
pool = db.get_pool("my.db", minconn=2, maxconn=10)

# 获取连接
conn = pool.getconn()
try:
    cur = conn.cursor()
    cur.execute("SELECT * FROM users")
    print(cur.fetchall())
finally:
    pool.putconn(conn)

# 或者使用上下文管理器
with pool.getconn() as conn:
    cur = conn.cursor()
    cur.execute("SELECT * FROM users")
    print(cur.fetchall())

# 关闭所有连接
pool.closeall()
```

## RAG 使用场景

```python
import build_my_db as db

# 创建 RAG 所需的表结构
with db.connect("rag.db") as conn:
    conn.execute("""
        CREATE TABLE IF NOT EXISTS chunks (
            id INT PRIMARY KEY,
            document_id INT,
            chunk_text TEXT,
            embedding_id INT,
            created_at TEXT
        )
    """)

    conn.execute("""
        CREATE TABLE IF NOT EXISTS embeddings (
            id INT PRIMARY KEY,
            chunk_id INT,
            vector_data BLOB,
            model TEXT
        )
    """)

    # 插入文档块
    chunks = [
        (1, 1, "机器学习是人工智能的一个分支...", 1, "2024-01-01"),
        (2, 1, "它使用数据来训练模型...", 2, "2024-01-01"),
        (3, 2, "深度学习使用多层神经网络...", 3, "2024-01-02"),
    ]

    cursor = conn.cursor()
    cursor.executemany(
        "INSERT INTO chunks VALUES (%s, %s, %s, %s, %s)",
        chunks
    )

    # 查询相关文档
    rows = conn.query("""
        SELECT c.id, c.chunk_text, d.title
        FROM chunks c
        JOIN documents d ON c.document_id = d.id
        WHERE c.chunk_text LIKE '%学习%'
    """)
```

## 注意事项

1. **CLI 工具路径**: 驱动会自动查找 `db_cli`，也可以通过 `BUILD_MY_DB_CLI` 环境变量指定
2. **JSON 输出模式**: 驱动使用 `--json` 参数调用 CLI，确保 CLI 已编译
3. **参数占位符**: 目前只支持 `%s` 格式（psycopg2 兼容）
4. **事务**: 默认每次 execute 后不会自动提交，需要手动调用 `commit()`

## 运行测试

```bash
cd engineering/apps/db_driver
pip install pytest
python -m pytest test_driver.py -v
```

## 运行示例

```bash
cd engineering/apps/db_driver
python examples.py
```