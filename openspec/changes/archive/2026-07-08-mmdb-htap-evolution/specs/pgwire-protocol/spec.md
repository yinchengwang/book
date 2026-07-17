# 规格：pgwire-protocol

## ADDED Requirements

### Requirement: PostgreSQL Wire Protocol 框架

系统 SHALL 实现 PostgreSQL Wire Protocol（pgwire）协议处理器。

#### Scenario: 连接握手
- **WHEN** 客户端连接服务器
- **THEN** 服务器 SHALL 发送 AuthenticationOk 或 AuthenticationMD5Password
- **THEN** 客户端认证完成后 SHALL 进入 Ready 状态

#### Scenario: 协议消息类型
- **WHEN** 处理协议消息
- **THEN** 前端消息（F）SHALL 被正确解析
- **THEN** 后端消息（B）SHALL 被正确发送
- **THEN** 消息类型包括：Parse, Bind, Describe, Execute, Sync, Terminate 等

#### Scenario: 协议状态机
- **WHEN** 协议处理中
- **THEN** Idle 状态 SHALL 处理新请求
- **THEN** Active 状态 SHALL 处理执行中请求
- **THEN** Error 状态 SHALL 处理错误情况

---

### Requirement: 简单查询协议

系统 SHALL 支持 PostgreSQL 简单查询协议（Simple Query Protocol）。

#### Scenario: Query 消息处理
- **WHEN** 收到 Query 消息
- **THEN** SQL 文本 SHALL 被解析
- **THEN** 查询 SHALL 被执行
- **THEN** RowDescription、DataRow、CommandComplete 消息 SHALL 被发送

#### Scenario: RowDescription 消息
- **WHEN** 返回结果集
- **THEN** RowDescription 消息 SHALL 描述列信息
- **THEN** 消息 SHALL 包含：字段数、字段名、字段 OID、列属性

#### Scenario: DataRow 消息
- **WHEN** 发送结果行
- **THEN** DataRow 消息 SHALL 包含每个字段的值
- **THEN** 值 SHALL 以二进制格式编码
- **THEN** NULL 值 SHALL 用 -1 表示长度

#### Scenario: CommandComplete 消息
- **WHEN** 查询执行完成
- **THEN** CommandComplete 消息 SHALL 发送
- **THEN** 消息 SHALL 包含命令标签（如 `SELECT 10`、`INSERT 0 1`）

---

### Requirement: 扩展查询协议

系统 SHALL 支持 PostgreSQL 扩展查询协议（Extended Query Protocol）。

#### Scenario: Parse 消息处理
- **WHEN** 收到 Parse 消息
- **THEN** SQL 语句 SHALL 被解析为预编译语句
- **THEN** 参数占位符（$1, $2）SHALL 被记录

#### Scenario: Bind 消息处理
- **WHEN** 收到 Bind 消息
- **THEN** 参数值 SHALL 被绑定到预编译语句
- **THEN** Portal SHALL 被创建

#### Scenario: Describe 消息处理
- **WHEN** 收到 Describe 消息
- **THEN** 预编译语句或 Portal 的信息 SHALL 被返回
- **THEN** RowDescription SHALL 被发送

#### Scenario: Execute 消息处理
- **WHEN** 收到 Execute 消息
- **THEN** Portal SHALL 被执行
- **THEN** 结果 SHALL 被发送

#### Scenario: Sync 消息处理
- **WHEN** 收到 Sync 消息
- **THEN** 当前事务 SHALL 被提交或回滚
- **THEN** 状态 SHALL 回到 Ready

---

### Requirement: 类型系统

系统 SHALL 实现 PostgreSQL 类型系统兼容。

#### Scenario: OID 类型映射
- **WHEN** 发送类型信息
- **THEN** 数据库类型 SHALL 被映射到 PostgreSQL OID
- **THEN** INTEGER → 23, BIGINT → 20, VARCHAR → 1043 等

#### Scenario: 二进制格式编码
- **WHEN** 发送数据值
- **THEN** INTEGER SHALL 使用网络字节序（big-endian）
- **THEN** VARCHAR SHALL 使用长度前缀 + 字符串
- **THEN** TIMESTAMP SHALL 使用微秒级 int64

#### Scenario: 数组类型支持
- **WHEN** 处理数组类型
- **THEN** 数组 SHALL 以 PostgreSQL 数组格式编码
- **THEN** 包含维度信息和元素值

#### Scenario: JSON 类型支持
- **WHEN** 处理 JSON 类型
- **THEN** JSON SHALL 以 text 格式发送
- **THEN** OID SHALL 为 114（JSON）

---

### Requirement: 事务支持

系统 SHALL 支持 PostgreSQL 事务命令。

#### Scenario: BEGIN 命令
- **WHEN** 收到 BEGIN
- **THEN** 事务 SHALL 被启动
- **THEN** ReadyForQuery 状态 SHALL 指示事务中

#### Scenario: COMMIT 命令
- **WHEN** 收到 COMMIT
- **THEN** 事务 SHALL 被提交
- **THEN** ReadyForQuery 状态 SHALL 指示空闲

#### Scenario: ROLLBACK 命令
- **WHEN** 收到 ROLLBACK
- **THEN** 事务 SHALL 被回滚
- **THEN** 所有更改 SHALL 被撤销

---

### Requirement: 错误处理

系统 SHALL 实现 PostgreSQL 错误响应。

#### Scenario: ErrorResponse 消息
- **WHEN** 执行出错
- **THEN** ErrorResponse 消息 SHALL 被发送
- **THEN** 消息 SHALL 包含错误级别（S）、错误代码（C）、错误信息（M）

#### Scenario: 错误代码
- **WHEN** 返回错误
- **THEN** 错误代码 SHALL 符合 SQLSTATE 标准
- **THEN** 42P01 → 表不存在, 42703 → 列不存在 等

#### Scenario: NoticeResponse 消息
- **WHEN** 有警告信息
- **THEN** NoticeResponse 消息 SHALL 被发送
- **THEN** 不影响命令执行

---

### Requirement: 协议兼容性

系统 SHALL 与标准 PostgreSQL 客户端兼容。

#### Scenario: psql 客户端兼容
- **WHEN** 使用 psql 连接
- **THEN** 基本查询 SHALL 能正常执行
- **THEN** 元命令（如 \d, \dt）SHALL 被支持（可选）

#### Scenario: 批量执行兼容
- **WHEN** 客户端发送多语句
- **THEN** 每条语句 SHALL 被依次执行
- **THEN** 结果 SHALL 被依次返回

#### Scenario: Prepared Statement 兼容
- **WHEN** 使用预编译语句
- **THEN** 语句 SHALL 被正确缓存
- **THEN** 后续执行 SHALL 使用缓存

#### Scenario: COPY 命令兼容
- **WHEN** 使用 COPY 命令
- **THEN** COPY IN/OUT SHALL 被支持（可选）
- **THEN** 二进制格式 SHALL 被支持
