# db_access 学习笔记

## 概念地图

Python 的数据库编程基于 DB-API 2.0 规范：

- **连接对象 `conn`**：管理数据库连接，支持 `commit()`/`rollback()`/`close()`
- **游标对象 `cursor`**：执行 SQL，返回结果集
- **参数化查询**：用 `?` 占位符，避免 SQL 注入（**必须使用**）
- **事务**：默认开启，显式 `commit()` 提交，`rollback()` 回滚
- **SQLite 特性**：单文件数据库，无服务器，适合本地存储

## 踩坑记录

1. **SQL 注入**：`"SELECT * FROM users WHERE id=" + user_id` 可被注入，用参数化查询
2. **忘记 commit**：修改数据后不 commit，连接关闭时自动回滚
3. **上下文管理器**：`with conn:` 自动处理 commit/rollback
4. **cursor 遍历**：fetchall() 一次性加载所有数据，大表用 fetchone() 分批
5. **并发写入**：SQLite 写锁粒度是数据库，同时写入会阻塞

## 工程对照（≥100 字硬约束）

Python 数据库编程在 `engineering/` 的存储引擎中有大量实践：

1. **SQLAlchemy ORM**：`engineering/` 的数据库代码可用 SQLAlchemy 封装为 ORM 模型
2. **连接池**：Web 应用（Flask/Django）用 `db_pool` 管理连接，避免频繁创建
3. **参数化查询**：所有 `engineering/src/db/` 中的 SQL 字符串都用参数化查询
4. **事务隔离级别**：PostgreSQL 的 `READ COMMITTED`/`SERIALIZABLE` 对应 SQLite 的不同行为
5. **游标迭代**：`for row in cursor:` 流式获取大结果集，内存友好
6. **SQLite 临时表**：用 `CREATE TEMP TABLE` 存储中间计算结果

学完本卡能动手的事：用 SQLite 实现一个简单的 TODO 应用，支持增删改查和搜索。
