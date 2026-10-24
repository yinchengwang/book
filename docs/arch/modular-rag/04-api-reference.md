# API 参考文档

> 版本：1.0
> 日期：2026-09-04

---

## 目录

1. [REST API 端点](#1-rest-api-端点)
2. [CLI 命令参考](#2-cli-命令参考)
3. [配置项说明](#3-配置项说明)

---

## 1. REST API 端点

### 1.1 基础信息

| 项目 | 说明 |
|------|------|
| Base URL | `http://localhost:8080/api/v1` |
| Content-Type | `application/json` |
| 字符编码 | UTF-8 |

### 1.2 健康检查端点

#### GET /health

服务健康状态检查。

**响应示例：**

```json
{
    "status": "ok",
    "version": "1.0.0"
}
```

#### GET /ready

服务就绪检查。

**响应示例：**

```json
{
    "status": "ready",
    "storage": true,
    "vector_index": true,
    "llm_loaded": true
}
```

#### GET /live

服务存活检查。

**响应示例：**

```json
{
    "status": "alive"
}
```

### 1.3 RAG 查询端点

#### POST /query

执行 RAG 查询。

**请求体：**

```json
{
    "text": "什么是 C++ 编程语言？",
    "pipeline": "naive",
    "top_k": 5,
    "rerank": true,
    "stream": false
}
```

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| text | string | 是 | 查询文本 |
| pipeline | string | 否 | Pipeline 类型，默认 naive |
| top_k | int | 否 | 检索数量，默认 5 |
| rerank | bool | 否 | 是否重排，默认 true |
| stream | bool | 否 | 是否流式返回，默认 false |

**响应示例（非流式）：**

```json
{
    "answer": "C++ 是一种高性能的编译型编程语言。",
    "context": [
        {
            "chunk_id": "doc1_chunk3",
            "content": "C++ 是一种通用编程语言，支持面向对象、泛型和过程式编程。",
            "score": 0.92,
            "source": "doc1.md"
        }
    ],
    "metadata": {
        "retrieval_time_ms": 45,
        "generation_time_ms": 120,
        "total_tokens": 512,
        "pipeline": "naive"
    }
}
```

#### POST /query/stream

流式 RAG 查询。

**请求体：** 同 `/query`，`stream` 字段被忽略（强制流式）。

**响应：** Server-Sent Events (SSE) 格式

```
data: {"type": "context", "chunk_id": "doc1_chunk3", "score": 0.92}
data: {"type": "token", "content": "C"}
data: {"type": "token", "content": "++"}
data: {"type": "token", "content": " 是"}
...
data: {"type": "done", "total_tokens": 512}
```

### 1.4 索引管理端点

#### POST /index

构建文档索引。

**请求体：**

```json
{
    "documents": [
        {
            "id": "doc1",
            "content": "C++ 是一种高性能编程语言。",
            "metadata": {
                "source": "wiki",
                "category": "programming"
            }
        }
    ],
    "chunk_size": 512,
    "chunk_overlap": 50,
    "embedding_model": "nomic-embed-text"
}
```

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| documents | array | 是 | 文档列表 |
| chunk_size | int | 否 | 分块大小，默认 512 |
| chunk_overlap | int | 否 | 分块重叠，默认 50 |
| embedding_model | string | 否 | Embedding 模型 |

**响应示例：**

```json
{
    "status": "success",
    "indexed_count": 1,
    "chunk_count": 3,
    "execution_time_ms": 1250
}
```

#### DELETE /index/{collection_id}

删除索引集合。

**路径参数：**

| 参数 | 类型 | 说明 |
|------|------|------|
| collection_id | string | 集合 ID |

**响应示例：**

```json
{
    "status": "success",
    "deleted_count": 150
}
```

### 1.5 Pipeline 管理端点

#### GET /pipelines

列出所有可用的 Pipeline。

**响应示例：**

```json
{
    "pipelines": [
        {
            "name": "naive",
            "description": "基础检索生成",
            "supported_features": ["stream", "rerank"]
        },
        {
            "name": "advanced",
            "description": "高级检索增强",
            "supported_features": ["stream", "rerank", "query_expansion"]
        }
    ]
}
```

#### GET /pipelines/{pipeline_name}

获取指定 Pipeline 详情。

**响应示例：**

```json
{
    "name": "advanced",
    "description": "高级检索增强",
    "config_schema": {
        "top_k": {"type": "int", "default": 10},
        "rerank_top_k": {"type": "int", "default": 5},
        "hybrid_alpha": {"type": "float", "default": 0.7}
    },
    "supported_features": ["stream", "rerank", "query_expansion"]
}
```

### 1.6 Agent 执行端点

#### POST /agent/execute

执行 Agent 命令。

**请求体：**

```json
{
    "command": "查询埃隆·马斯克的父亲是谁",
    "agent_type": "react",
    "max_iterations": 10,
    "tools": ["retrieve", "knowledge_graph", "calculator"]
}
```

| 字段 | 类型 | 必需 | 说明 |
|------|------|------|------|
| command | string | 是 | 用户命令 |
| agent_type | string | 否 | Agent 类型，默认 react |
| max_iterations | int | 否 | 最大迭代次数，默认 10 |
| tools | array | 否 | 启用的工具列表 |

**响应示例：**

```json
{
    "answer": "埃隆·马斯克的父亲是埃罗尔·马斯克。",
    "steps": [
        {
            "step_number": 1,
            "thought": "我需要先查询埃隆·马斯克的信息",
            "action": "knowledge_graph",
            "observation": "埃隆·马斯克是特斯拉 CEO"
        },
        {
            "step_number": 2,
            "thought": "现在我需要找到他父亲的信息",
            "action": "knowledge_graph",
            "observation": "他的父亲是埃罗尔·马斯克"
        }
    ],
    "total_steps": 2,
    "execution_time_ms": 3500,
    "success": true
}
```

### 1.7 系统状态端点

#### GET /status

获取系统状态。

**响应示例：**

```json
{
    "status": "running",
    "uptime_seconds": 3600,
    "version": "1.0.0",
    "storage": {
        "total_documents": 1500,
        "total_chunks": 8500,
        "vector_count": 8500
    },
    "llm": {
        "model": "llama-2-7b-chat.gguf",
        "loaded": true,
        "memory_used_mb": 4096
    }
}
```

#### GET /metrics

Prometheus 格式指标。

**响应：**

```
# HELP minivecdb_vectors_total 向量总数
# TYPE minivecdb_vectors_total gauge
minivecdb_vectors_total 8500

# HELP minivecdb_query_total 查询总数
# TYPE minivecdb_query_total counter
minivecdb_query_total 1250

# HELP minivecdb_query_duration_seconds 查询延迟
# TYPE minivecdb_query_duration_seconds histogram
minivecdb_query_duration_seconds_bucket{le="0.1"} 1000
minivecdb_query_duration_seconds_bucket{le="0.5"} 1200
minivecdb_query_duration_seconds_bucket{le="1.0"} 1240
minivecdb_query_duration_seconds_bucket{le="+Inf"} 1250
```

---

## 2. CLI 命令参考

### 2.1 命令列表

| 命令 | 说明 |
|------|------|
| `modular-rag start` | 启动 RAG 服务 |
| `modular-rag index` | 构建文档索引 |
| `modular-rag query` | 执行查询 |
| `modular-rag agent` | 执行 Agent 命令 |
| `modular-rag list-pipelines` | 列出可用 Pipeline |
| `modular-rag status` | 查看系统状态 |
| `modular-rag config` | 管理配置 |
| `modular-rag version` | 显示版本信息 |

### 2.2 启动服务

```bash
# 启动服务（默认配置）
modular-rag start

# 指定配置和端口
modular-rag start --config /path/to/config.yaml --port 8080

# 后台运行
modular-rag start --daemon

# 指定日志级别
modular-rag start --log-level debug
```

**参数：**

| 参数 | 说明 | 默认值 |
|------|------|--------|
| --config | 配置文件路径 | config/default.yaml |
| --port | 服务端口 | 8080 |
| --host | 服务地址 | 0.0.0.0 |
| --log-level | 日志级别 | info |
| --daemon | 后台运行 | false |

### 2.3 索引构建

```bash
# 索引单个文档
modular-rag index --input ./docs/introduction.md

# 索引目录
modular-rag index --input ./docs --recursive

# 指定集合名称
modular-rag index --input ./docs --collection my-docs

# 指定分块参数
modular-rag index --input ./docs --chunk-size 1024 --chunk-overlap 128

# 指定 Pipeline
modular-rag index --input ./docs --pipeline advanced

# 增量索引
modular-rag index --input ./docs --mode incremental
```

**参数：**

| 参数 | 说明 | 默认值 |
|------|------|--------|
| --input | 输入文件或目录 | - |
| --collection | 集合名称 | default |
| --chunk-size | 分块大小 | 512 |
| --chunk-overlap | 分块重叠 | 50 |
| --pipeline | 索引 Pipeline | naive |
| --mode | 索引模式 | full |
| --recursive | 递归处理目录 | false |

### 2.4 执行查询

```bash
# 基本查询
modular-rag query --text "什么是 C++？"

# 指定 Pipeline
modular-rag query --text "什么是 C++？" --pipeline advanced

# 指定返回数量
modular-rag query --text "什么是 C++？" --top-k 10

# 流式输出
modular-rag query --text "什么是 C++？" --stream

# 输出上下文
modular-rag query --text "什么是 C++？" --show-context
```

**参数：**

| 参数 | 说明 | 默认值 |
|------|------|--------|
| --text | 查询文本 | - |
| --pipeline | Pipeline 类型 | naive |
| --top-k | 返回结果数 | 5 |
| --stream | 流式输出 | false |
| --show-context | 显示上下文 | false |

### 2.5 Agent 命令

```bash
# 执行 Agent 命令
modular-rag agent --command "查询特斯拉 CEO 的父亲是谁"

# 指定 Agent 类型
modular-rag agent --command "查询天气" --agent-type react

# 指定工具
modular-rag agent --command "计算 2+2" --tools calculator

# 最大迭代次数
modular-rag agent --command "分析这个问题" --max-iterations 20
```

**参数：**

| 参数 | 说明 | 默认值 |
|------|------|--------|
| --command | Agent 命令 | - |
| --agent-type | Agent 类型 | react |
| --tools | 启用的工具 | all |
| --max-iterations | 最大迭代 | 10 |

### 2.6 列出 Pipeline

```bash
# 列出所有 Pipeline
modular-rag list-pipelines

# 显示详细信息
modular-rag list-pipelines --verbose

# 过滤 Pipeline
modular-rag list-pipelines --filter "advanced"
```

### 2.7 系统状态

```bash
# 查看状态
modular-rag status

# 详细信息
modular-rag status --verbose

# JSON 格式
modular-rag status --json
```

### 2.8 配置管理

```bash
# 显示当前配置
modular-rag config show

# 验证配置
modular-rag config validate --file /path/to/config.yaml

# 生成默认配置
modular-rag config init --output ./config.yaml
```

---

## 3. 配置项说明

### 3.1 配置格式（YAML）

```yaml
# Modular RAG 配置文件
rag:
  pipeline: "advanced"              # Pipeline 类型
  log_level: "info"                # 日志级别

llm:
  model_path: "./models/llama-2-7b-chat.gguf"
  n_ctx: 4096                      # 上下文窗口大小
  n_threads: 4                     # CPU 线程数
  temperature: 0.7                 # 生成温度
  max_tokens: 2048                 # 最大生成长度
  stream: false                   # 流式输出

embedding:
  model: "nomic-embed-text"       # Embedding 模型
  dimension: 768                  # 向量维度
  batch_size: 32                  # 批处理大小
  normalize: true                 # 是否归一化

retrieval:
  top_k: 10                       # 检索数量
  rerank_top_k: 5                 # 重排后数量
  hybrid_alpha: 0.7              # Vector/BM25 权重
  enable_rerank: true            # 是否启用重排
  enable_bm25: true              # 是否启用 BM25

storage:
  db_path: "./data/rag.db"       # 数据库路径
  vector_index: "hnsw"           # 向量索引类型
  bm25_enabled: true             # 启用 BM25
  graph_enabled: true            # 启用图存储

agent:
  type: "react"                  # Agent 类型
  max_iterations: 10             # 最大迭代
  timeout_ms: 60000              # 超时时间
  confidence_threshold: 0.7       # 置信度阈值

memory:
  short_term_size: 50             # 短期记忆大小
  long_term_enabled: true        # 启用长期记忆
  session_history: 100           # 会话历史大小

server:
  host: "0.0.0.0"               # 服务地址
  port: 8080                     # 服务端口
  workers: 4                     # 工作线程数
  max_request_size: 10485760    # 最大请求大小
```

### 3.2 配置项详解

#### 3.2.1 RAG 配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| rag.pipeline | string | naive | Pipeline 类型 |
| rag.log_level | string | info | 日志级别 |

#### 3.2.2 LLM 配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| llm.model_path | string | - | 模型文件路径 |
| llm.n_ctx | int | 4096 | 上下文窗口 |
| llm.n_threads | int | 4 | CPU 线程数 |
| llm.temperature | float | 0.7 | 生成温度 |
| llm.max_tokens | int | 2048 | 最大 token 数 |
| llm.stream | bool | false | 流式输出 |
| llm.n_gpu_layers | int | 0 | GPU 加速层数 |

#### 3.2.3 Embedding 配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| embedding.model | string | nomic-embed-text | 模型名称 |
| embedding.dimension | int | 768 | 向量维度 |
| embedding.batch_size | int | 32 | 批处理大小 |
| embedding.normalize | bool | true | 是否归一化 |

#### 3.2.4 检索配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| retrieval.top_k | int | 10 | 检索数量 |
| retrieval.rerank_top_k | int | 5 | 重排后数量 |
| retrieval.hybrid_alpha | float | 0.7 | Vector/BM25 融合权重 |
| retrieval.enable_rerank | bool | true | 启用重排 |
| retrieval.enable_bm25 | bool | true | 启用 BM25 |

#### 3.2.5 存储配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| storage.db_path | string | ./data/rag.db | 数据库路径 |
| storage.vector_index | string | hnsw | 向量索引类型 |
| storage.bm25_enabled | bool | true | 启用 BM25 |
| storage.graph_enabled | bool | true | 启用图存储 |

#### 3.2.6 Agent 配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| agent.type | string | react | Agent 类型 |
| agent.max_iterations | int | 10 | 最大迭代次数 |
| agent.timeout_ms | int | 60000 | 超时时间(ms) |
| agent.confidence_threshold | float | 0.7 | 置信度阈值 |

#### 3.2.7 Memory 配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| memory.short_term_size | int | 50 | 短期记忆大小 |
| memory.long_term_enabled | bool | true | 启用长期记忆 |
| memory.session_history | int | 100 | 会话历史大小 |

#### 3.2.8 服务配置

| 配置项 | 类型 | 默认值 | 说明 |
|--------|------|--------|------|
| server.host | string | 0.0.0.0 | 服务地址 |
| server.port | int | 8080 | 服务端口 |
| server.workers | int | 4 | 工作线程数 |
| server.max_request_size | int | 10485760 | 最大请求大小 |

---

## 4. 错误码

### 4.1 HTTP 状态码

| 状态码 | 说明 |
|--------|------|
| 200 | 成功 |
| 201 | 创建成功 |
| 400 | 请求参数错误 |
| 404 | 资源不存在 |
| 500 | 服务器内部错误 |
| 503 | 服务不可用 |

### 4.2 业务错误码

```json
{
    "error": {
        "code": "RAG_001",
        "message": "索引构建失败",
        "details": "文档分块失败"
    }
}
```

| 错误码 | 说明 |
|--------|------|
| RAG_001 | 索引构建失败 |
| RAG_002 | 检索失败 |
| RAG_003 | LLM 生成失败 |
| RAG_004 | Pipeline 不存在 |
| AGENT_001 | Agent 执行失败 |
| AGENT_002 | 工具执行失败 |
| AGENT_003 | 内存不足 |
| STORAGE_001 | 存储初始化失败 |
| STORAGE_002 | 存储读写错误 |

---

## 5. 相关文档

- [概述](./01-overview.md) - 项目概述、架构、9 种 Pipeline 总览
- [Pipeline 详细指南](./02-pipeline-guide.md) - 9 种 Pipeline 详细介绍
- [Agent 框架指南](./03-agent-guide.md) - Tool 系统、Memory 系统、ReAct 循环
