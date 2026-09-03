# rest-api Specification

## Purpose
MiniVecDB REST API 规范，支持向量数据库的集合管理、向量 CRUD、向量搜索和监控接口。本规范由 mmdb-htap-evolution 和 minivecdb-production-readiness 变更合并而成。

## Requirements
### Requirement: HTTP 服务器

系统 SHALL 实现轻量级 HTTP 服务器。

#### Scenario: HTTP 请求处理
- **WHEN** 收到 HTTP 请求
- **THEN** 请求 SHALL 被正确解析
- **THEN** 路由 SHALL 被匹配
- **THEN** 响应 SHALL 被返回

#### Scenario: JSON 序列化
- **WHEN** 返回 JSON 响应
- **THEN** 数据 SHALL 被序列化为 JSON 格式
- **THEN** Content-Type SHALL 设置为 application/json

#### Scenario: 错误响应
- **WHEN** 处理请求出错
- **THEN** 错误 SHALL 以 JSON 格式返回
- **THEN** HTTP 状态码 SHALL 反映错误类型

---

### Requirement: 查询 API

系统 SHALL 提供 SQL 查询接口。

#### Scenario: SQL 执行接口
- **WHEN** POST /api/v1/query
- **WHEN** 请求体包含 `{"sql": "SELECT * FROM users"}`
- **THEN** SQL SHALL 被执行
- **THEN** 结果 SHALL 以 JSON 数组返回

#### Scenario: 参数化查询
- **WHEN** POST /api/v1/query
- **WHEN** 请求体包含参数
- **THEN** 参数 SHALL 被正确绑定
- **THEN** SQL 注入 SHALL 被防止

#### Scenario: 结果分页
- **WHEN** 查询返回大量结果
- **THEN** 支持 limit 和 offset 参数
- **THEN** 返回分页结果和总数

---

### Requirement: 管理 API

系统 SHALL 提供集合管理接口。

#### Scenario: 列出集合
- **WHEN** GET /api/v1/collections
- **THEN** 所有集合 SHALL 被列出
- **THEN** 返回集合名称和元数据

#### Scenario: 创建集合
- **WHEN** POST /api/v1/collections
- **WHEN** 请求体包含集合定义
- **THEN** 新集合 SHALL 被创建
- **THEN** 集合 ID SHALL 被返回

#### Scenario: 删除集合
- **WHEN** DELETE /api/v1/collections/{name}
- **THEN** 指定集合 SHALL 被删除
- **THEN** 确认消息 SHALL 被返回

---

### Requirement: 向量搜索 API

系统 SHALL 提供向量搜索接口。

#### Scenario: 插入向量
- **WHEN** POST /api/v1/vectors/{collection}
- **WHEN** 请求体包含向量数据
- **THEN** 向量 SHALL 被插入
- **THEN** 向量 ID SHALL 被返回

#### Scenario: 向量搜索
- **WHEN** POST /api/v1/vectors/{collection}/search
- **WHEN** 请求体包含查询向量和参数
- **THEN** 最近邻向量 SHALL 被搜索
- **THEN** 结果 SHALL 包含 ID 和距离

#### Scenario: 混合搜索
- **WHEN** POST /api/v1/vectors/{collection}/search
- **WHEN** 请求体包含过滤条件
- **THEN** 过滤 SHALL 与向量搜索组合

---

### Requirement: 时序 API

系统 SHALL 提供时序数据接口。

#### Scenario: 插入时序数据
- **WHEN** POST /api/v1/timeseries/{collection}
- **WHEN** 请求体包含时序数据点
- **THEN** 数据点 SHALL 被插入

#### Scenario: 时序聚合查询
- **WHEN** POST /api/v1/timeseries/{collection}/aggregate
- **WHEN** 请求体包含聚合参数
- **THEN** 聚合结果 SHALL 被返回

---

### Requirement: 监控 API

系统 SHALL 提供监控和健康检查接口。

#### Scenario: 健康检查
- **WHEN** GET /api/v1/health
- **THEN** 服务健康状态 SHALL 被返回
- **THEN** 状态码为 200 表示正常

#### Scenario: 指标查询
- **WHEN** GET /api/v1/metrics
- **THEN** 系统指标 SHALL 被返回
- **THEN** 指标包括 QPS、延迟、内存使用等

#### Scenario: 状态查询
- **WHEN** GET /api/v1/status
- **THEN** 运行时状态 SHALL 被返回
- **THEN** 包括连接数、活动事务数等

---

### Requirement: API 认证（可选）

系统 SHALL 支持可选的 API 认证。

#### Scenario: API Key 认证
- **WHEN** 请求包含 X-API-Key 头
- **THEN** API Key SHALL 被验证
- **THEN** 无效 Key SHALL 返回 401

#### Scenario: CORS 支持
- **WHEN** 收到跨域请求
- **THEN** CORS 头 SHALL 被正确设置
- **THEN** 允许配置的源访问

