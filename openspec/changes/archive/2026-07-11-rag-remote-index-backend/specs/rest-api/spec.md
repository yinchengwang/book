## MODIFIED Requirements

### Requirement: 管理 API

系统 SHALL 提供集合管理接口。

#### Scenario: 列出集合
- **WHEN** `GET /collections`
- **THEN** 所有集合 SHALL 被列出
- **THEN** 返回集合名称和元数据

#### Scenario: 创建集合
- **WHEN** `POST /collections`
- **WHEN** 请求体包含 `name`、`dimension`、可选的 `index_type`、`metric_type`
- **THEN** 新集合 SHALL 被创建
- **THEN** VectorAPI SHALL 被真实调用（非桩代码）
- **THEN** 集合 ID SHALL 被返回
- **THEN** 响应 SHALL 包含 `"index_type"` 字段

#### Scenario: 删除集合
- **WHEN** `DELETE /collections/{name}`
- **THEN** VectorAPI SHALL 被真实调用删除集合
- **THEN** 关联的 BM25 索引 SHALL 同时被销毁
- **THEN** 确认消息 SHALL 被返回

### Requirement: 向量搜索 API

系统 SHALL 提供向量搜索接口。

#### Scenario: 插入向量
- **WHEN** `POST /collections/:id/vectors`
- **WHEN** 请求体包含 `vectors` 数组
- **THEN** VectorAPI SHALL 被真实调用插入向量
- **THEN** 插入计数 SHALL 被返回

#### Scenario: 向量搜索
- **WHEN** `POST /collections/:id/search`
- **WHEN** 请求体包含 `vector` 和 `top_k`
- **THEN** VectorAPI SHALL 被真实调用执行搜索
- **THEN** 结果 SHALL 包含 `id` 和 `score`

## ADDED Requirements

### Requirement: 文本搜索 API（BM25）

系统 SHALL 提供 BM25 文本搜索接口。

#### Scenario: BM25 添加文本
- **WHEN** `POST /collections/:id/text-search`
- **WHEN** 请求体包含 `"action": "add"`、`"id"`、`"text"`
- **THEN** BM25 索引 SHALL 添加文档
- **THEN** 确认消息 SHALL 被返回

#### Scenario: BM25 文本搜索
- **WHEN** `POST /collections/:id/text-search`
- **WHEN** 请求体包含 `"query"`、`"top_k"`
- **THEN** BM25 索引 SHALL 被搜索
- **THEN** 结果 SHALL 包含 `id`、`score`、`text`

#### Scenario: BM25 删除文本
- **WHEN** `POST /collections/:id/text-search`
- **WHEN** 请求体包含 `"action": "delete"`、`"id"`
- **THEN** BM25 索引 SHALL 删除文档

### Requirement: 向量删除 API

系统 SHALL 支持通过 REST API 删除向量。

#### Scenario: 删除向量
- **WHEN** `DELETE /collections/{id}/vectors/{vid}`
- **THEN** VectorAPI SHALL 被真实调用删除向量

### Requirement: 向量获取 API

系统 SHALL 支持通过 REST API 获取单个向量。

#### Scenario: 获取向量
- **WHEN** `GET /collections/{id}/vectors/{vid}`
- **THEN** VectorAPI SHALL 被真实调用获取向量
- **THEN** 响应 SHALL 包含 `id` 和 `vector` 字段
