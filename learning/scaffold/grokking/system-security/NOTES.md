# 安全设计 学习笔记

## 核心概念

- **认证 (Authentication)**: 验证"你是谁"——JWT/Session/OAuth2
- **授权 (Authorization)**: 验证"你能做什么"——RBAC/ABAC
- **HTTPS/TLS**: 传输层加密，防止中间人攻击
- **加密存储**: 密码应加盐哈希存储（bcrypt/argon2）

## 安全防御层次

| 层次 | 措施 | 防御目标 |
|------|------|----------|
| 传输层 | TLS 1.3, HSTS | 窃听/篡改 |
| 应用层 | 参数化查询/CSRF Token | 注入/跨站 |
| 认证层 | MFA/OAuth2 | 身份仿冒 |
| 授权层 | RBAC/最小权限 | 越权访问 |
| 数据层 | 加密/Audit Log | 数据泄露 |

## 工程对照

`engineering/src/db/core/db_server.c` 中的数据库服务器实现了简单的身份验证和会话管理——客户端连接时需提供有效的认证信息，成功后才能执行 SQL 查询。`engineering/include/db/guc.h` 中的配置参数包括 `listen_addresses`（绑定地址）和 `port`（端口），这对应安全设计中的"攻击面最小化"原则——只监听必要的 IP 和端口。`engineering/src/db/lock/lock.c` 中的锁机制对数据和操作进行隔离，防止并发操作造成的数据不一致，这对应安全设计中的"资源隔离"策略。`engineering/src/db/wal/wal.c` 中的 WAL 日志的功能包括事务完整性保障——在系统崩溃后通过日志重放恢复数据，这对应安全设计中的"数据完整性"要求。`engineering/include/db/storage_engine.h` 中定义了 `mm_storage_t` 多模态存储引擎，其访问接口可以通过扩展实现细粒度的权限控制——安全设计中的"最小权限"原则在存储层的映射。

## 面试要点

1. HTTPS 必须在负载均衡器上做 SSL 终结，证书管理是运维核心
2. 防御深度（Defense in Depth）——多层防护而非依赖单一防线
3. 任何用户输入都不可信——验证/净化/参数化为三原则
