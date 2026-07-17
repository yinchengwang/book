## ADDED Requirements

### Requirement: BM25Index 接口抽象化

RAG Engine SHALL 提供 `IBm25Index` 抽象基类，BM25Index（本地实现）和 RemoteBM25Index（远程实现）均继承自它。

#### Scenario: IBm25Index 接口
- **WHEN** RAG Engine 创建索引实例
- **THEN** `bm25_index_` 成员类型 SHALL 为 `std::shared_ptr<IBm25Index>`
- **THEN** LOCAL 模式 SHALL 创建 `BM25Index`，REMOTE 模式 SHALL 创建 `RemoteBM25Index`

### Requirement: RemoteVectorIndex

RAG Engine SHALL 提供 `RemoteVectorIndex` 类，通过 HTTP 远程调用 REST Server 实现 `VectorIndex` 接口。

#### Scenario: 远程搜索
- **WHEN** `RemoteVectorIndex::search(query_vector, top_k)` 被调用
- **THEN** SHALL 发送 `POST /collections/{name}/search` 请求
- **THEN** 请求体 SHALL 包含 `vector` 和 `top_k` 字段
- **THEN** 响应 SHALL 被解析为 `vector<pair<string, float>>` 返回

#### Scenario: 远程插入
- **WHEN** `RemoteVectorIndex::add(id, vector)` 被调用
- **THEN** SHALL 发送 `POST /collections/{name}/vectors` 请求
- **THEN** 请求体 SHALL 包含 `vectors` 数组

#### Scenario: 连接失败处理
- **WHEN** REST Server 不可用
- **THEN** SHALL 重试 3 次，间隔 500ms
- **THEN** 全部失败后 SHALL 返回空结果并记录错误

#### Scenario: 远程不适用方法的默认处理
- **WHEN** `RemoteVectorIndex::init/save/load/clear` 被调用
- **THEN** SHALL 继承 `VectorIndex` 接口的默认空实现（no-op）
- **THEN** SHALL 记录 LOG 调用信息

#### Scenario: ID 类型映射
- **WHEN** `RemoteVectorIndex::add(id, vector)` 被调用
- **THEN** SHALL 为 string ID 分配自增 int64，存入本地映射表
- **WHEN** `RemoteVectorIndex::search()` 返回结果
- **THEN** SHALL 查映射表将 int64 转回 string 返回给调用方

### Requirement: RemoteBM25Index

RAG Engine SHALL 提供 `RemoteBM25Index` 类，通过 HTTP 远程调用 REST Server 实现 `IBm25Index` 接口。

#### Scenario: 远程 BM25 搜索
- **WHEN** `RemoteBM25Index::search(query, top_k)` 被调用
- **THEN** SHALL 发送 `POST /collections/{name}/text-search` 请求
- **THEN** 请求体 SHALL 包含 `query` 和 `top_k` 字段

#### Scenario: 远程 BM25 添加文档
- **WHEN** `RemoteBM25Index::add(id, text)` 被调用
- **THEN** SHALL 发送 `POST /collections/{name}/text-search` 请求（含 `action: "add"`）

### Requirement: 索引后端模式切换

RAG Engine SHALL 支持 `LOCAL` 和 `REMOTE` 两种索引后端模式，通过配置选择。

#### Scenario: REMOTE 模式启动
- **WHEN** `EngineConfig.index_backend = REMOTE`
- **THEN** `init_components()` SHALL 创建 `RemoteVectorIndex` 和 `RemoteBM25Index`
- **THEN** SHALL 使用 `config.rest_url` 作为 REST Server 地址（默认 `http://localhost:8080`）

#### Scenario: LOCAL 模式保留
- **WHEN** `EngineConfig.index_backend = LOCAL`
- **THEN** `init_components()` SHALL 创建原有的本地 `HNSWIndex` 和 `BM25Index`
- **THEN** 行为与改前一致

### Requirement: 集合自动初始化

RAG Server 启动时 SHALL 自动在 REST Server 创建所需集合。

#### Scenario: 自动创建集合
- **WHEN** RAG Server 启动（REMOTE 模式）
- **THEN** SHALL 调用 `GET /collections/{name}` 检查集合是否存在
- **THEN** 集合不存在时 SHALL 调用 `POST /collections` 创建
- **THEN** 集合维度、索引类型从 `HNSWConfig` 读取
