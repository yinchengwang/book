# db_access scaffold

Python SQLite 数据库访问演示——连接/创建表/增删改查/事务。

## 复现命令

```bash
cd learning/scaffold/python/db_access
python main.py
```

或使用 Makefile：

```bash
make run
```

## 预期输出（节选）

```
[connect] === 连接 SQLite 数据库 ===
  连接成功: /tmp/xxx.db
  SQLite 版本: 3.x.x

[create] === 创建表 ===
  创建表: users, posts

[insert] === 插入数据 ===
  插入用户，ID: 1
  批量插入: 3 条

[transaction] === 事务处理 ===
  事务回滚: FOREIGN KEY constraint failed

=== PASS ===
```

## 关键点

- **`sqlite3.connect()`**：创建数据库连接
- **`cursor.execute()`**：执行 SQL 语句
- **参数化查询**：用 `?` 占位符防止 SQL 注入
- **`conn.commit()`**：提交事务
- **`conn.rollback()`**：回滚事务

详见 NOTES.md 工程对照段。
