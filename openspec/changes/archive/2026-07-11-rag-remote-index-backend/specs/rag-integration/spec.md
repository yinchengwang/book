## MODIFIED Requirements

### Requirement: RAG Demo 应用

MiniVecDB SHALL provide a complete RAG (Retrieval-Augmented Generation) demonstration application.

#### Scenario: Demo 应用功能
- **WHEN** 用户运行 RAG Demo
- **THEN** Demo SHALL 提供以下功能：
  - 文档上传和分块
  - 向量化存储（通过 REST Server）
  - 语义搜索（通过 REST Server）
  - 与 LLM 集成（支持 OpenAI API）
  - 生成答案输出

#### Scenario: 技术栈
- **WHEN** Demo 应用启动时
- **THEN** 系统 SHALL 使用：
  - Engineering REST Server 作为向量数据库后端
  - Python SDK 通过 HTTP 与 REST Server 交互
  - RAG Server（C++）做文档编排和 LLM 生成
  - 任意流行 LLM API（OpenAI GPT / Claude / 本地模型）

### Requirement: 搜索增强流程

Demo SHALL demonstrate the full RAG search and generation flow using remote index backend.

#### Scenario: 语义搜索
- **WHEN** 用户输入查询时
- **THEN** 系统 SHALL：
  - 将查询向量化
  - 在 REST Server 中搜索最相关的文档块（`POST /collections/{name}/search`）
  - 返回 top_k 个结果（默认 5）

#### Scenario: 上下文构建
- **WHEN** 搜索结果返回时
- **THEN** 系统 SHALL：
  - 将相关文档块组合为上下文
  - 包含引用来源信息
  - 遵守 LLM 的上下文长度限制

#### Scenario: LLM 生成
- **WHEN** 上下文构建完成后
- **THEN** 系统 SHALL：
  - 调用 LLM API 生成答案
  - 传递上下文和用户问题
  - 返回生成的回答和引用来源

## ADDED Requirements

### Requirement: 远程索引后端支持

RAG 系统 SHALL 支持通过 REST Server 进行远程索引操作。

#### Scenario: RAG Engine REMOTE 模式
- **WHEN** RAG Engine 以 REMOTE 模式启动
- **THEN** 向量搜索 SHALL 使用 `RemoteVectorIndex`
- **THEN** BM25 检索 SHALL 使用 `RemoteBM25Index`
- **THEN** 查询流程：嵌入 → HTTP 搜索(REST) → RRF 融合 → LLM 生成

#### Scenario: 索引初始化
- **WHEN** RAG Engine 初始化（REMOTE 模式）
- **THEN** SHALL 自动在 REST Server 创建所需集合
- **THEN** 集合命名规则 SHALL 为 `rag_{index_name}`
