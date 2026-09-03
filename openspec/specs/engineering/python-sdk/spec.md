# python-sdk Specification

## Purpose
MiniVecDB Python SDK 规范，提供 Pythonic 的向量数据库客户端。本规范由 mmdb-htap-evolution 和 minivecdb-production-readiness 变更合并而成。

## Requirements
### Requirement: Python SDK 连接管理

系统 SHALL 实现 Python SDK 的连接管理功能。

#### Scenario: 建立连接
- **WHEN** 调用 `mmdb.connect(host, port, user, password)`
- **THEN** 与服务器的连接 SHALL 被建立
- **THEN** 返回 Connection 对象

#### Scenario: 连接池
- **WHEN** 使用连接池
- **THEN** 连接 SHALL 被池化复用
- **THEN** 最大连接数 SHALL 可配置

#### Scenario: 连接关闭
- **WHEN** 调用 `connection.close()`
- **THEN** 连接 SHALL 被正确关闭
- **THEN** 资源 SHALL 被释放

#### Scenario: 自动重连
- **WHEN** 连接断开
- **THEN** SDK SHALL 支持自动重连
- **THEN** 重试次数 SHALL 可配置

---

### Requirement: Cursor 和查询执行

系统 SHALL 实现游标和查询执行功能。

#### Scenario: 创建游标
- **WHEN** 调用 `connection.cursor()`
- **THEN** Cursor 对象 SHALL 被返回

#### Scenario: 执行查询
- **WHEN** 调用 `cursor.execute(sql)`
- **THEN** SQL SHALL 被发送到服务器执行
- **THEN** 结果 SHALL 被缓存

#### Scenario: 获取结果
- **WHEN** 调用 `cursor.fetchone()`
- **THEN** 单行结果 SHALL 被返回
- **WHEN** 调用 `cursor.fetchall()`
- **THEN** 所有结果 SHALL 被返回

#### Scenario: 参数化查询
- **WHEN** 调用 `cursor.execute(sql, params)`
- **THEN** 参数 SHALL 被正确绑定
- **THEN** SQL 注入 SHALL 被防止

#### Scenario: 批量执行
- **WHEN** 调用 `cursor.executemany(sql, params_list)`
- **THEN** SQL SHALL 被批量执行

---

### Requirement: 事务支持

系统 SHALL 实现事务管理。

#### Scenario: 显式事务
- **WHEN** 调用 `connection.begin()`
- **THEN** 事务 SHALL 被开启
- **WHEN** 调用 `connection.commit()`
- **THEN** 事务 SHALL 被提交
- **WHEN** 调用 `connection.rollback()`
- **THEN** 事务 SHALL 被回滚

#### Scenario: 自动提交
- **WHEN** 设置 `connection.autocommit = True`
- **THEN** 每个语句 SHALL 自动提交

---

### Requirement: 类型映射

系统 SHALL 实现 Python 类型与数据库类型的映射。

#### Scenario: 整数映射
- **WHEN** 读取 INTEGER 列
- **THEN** Python int SHALL 被返回

#### Scenario: 字符串映射
- **WHEN** 读取 VARCHAR 列
- **THEN** Python str SHALL 被返回

#### Scenario: 浮点数映射
- **WHEN** 读取 DOUBLE 列
- **THEN** Python float SHALL 被返回

#### Scenario: NULL 映射
- **WHEN** 读取 NULL 值
- **THEN** Python None SHALL 被返回

#### Scenario: 数组映射
- **WHEN** 读取数组列
- **THEN** Python list SHALL 被返回

#### Scenario: JSON 映射
- **WHEN** 读取 JSON 列
- **THEN** Python dict/list SHALL 被返回

---

### Requirement: 向量操作

系统 SHALL 提供向量操作接口。

#### Scenario: 插入向量
- **WHEN** 调用向量插入方法
- **THEN** 浮点数组 SHALL 被发送到服务器

#### Scenario: 向量搜索
- **WHEN** 调用向量搜索方法
- **THEN** 搜索结果 SHALL 包含 ID 和距离

#### Scenario: 混合搜索
- **WHEN** 调用混合搜索方法
- **THEN** 过滤条件和向量搜索 SHALL 被组合

---

### Requirement: 上下文管理器

系统 SHALL 支持 Python 上下文管理器。

#### Scenario: Connection 上下文
- **WHEN** 使用 `with mmdb.connect(...) as conn:`
- **THEN** 退出时连接 SHALL 被自动关闭

#### Scenario: Cursor 上下文
- **WHEN** 使用 `with conn.cursor() as cur:`
- **THEN** 退出时游标 SHALL 被自动关闭

---

### Requirement: 异步支持（可选）

系统 SHALL 支持异步操作。

#### Scenario: 异步连接
- **WHEN** 使用 `await mmdb.connect_async(...)`
- **THEN** 异步连接 SHALL 被建立

#### Scenario: 异步执行
- **WHEN** 使用 `await cursor.execute_async(sql)`
- **THEN** 异步执行 SHALL 被支持

