# RAG 系统接口设计

## 概述

本文档定义 RAG 系统的 CLI 命令和 REST API 接口规范。

---

## 1. CLI 命令设计

### 1.1 命令结构

```
rag [全局选项] <命令> [子命令] [参数]
```

### 1.2 全局选项

| 选项 | 说明 | 默认值 |
|------|------|--------|
| `--config <path>` | 配置文件路径 | `./rag-config.yaml` |
| `--verbose, -v` | 输出详细日志 | false |
| `--quiet, -q` | 静默模式，只输出错误 | false |
| `--log-level <level>` | 日志级别 (trace/debug/info/warn/error) | info |
| `--output <format>` | 输出格式 (text/json/yaml) | text |

### 1.3 命令列表

#### 1.3.1 index - 索引管理

```bash
# 构建索引
rag index build [选项]

选项:
  --path <dir>          # 指定要索引的目录 (可多次指定)
  --file <file>         # 指定单个文件 (可多次指定)
  --pattern <glob>      # 文件匹配模式
  --incremental         # 增量索引 (只处理变更文件)
  --full                # 全量重建索引
  --workers <n>         # 并行工作线程数 (默认: 4)
  --batch-size <n>      # 批处理大小 (默认: 32)
  --force, -f           # 强制重建，跳过变更检测

示例:
  rag index build                           # 使用默认配置构建索引
  rag index build --path ./docs             # 索引 docs 目录
  rag index build --path ./src --pattern "**/*.cpp"  # 只索引 C++ 文件
  rag index build --incremental             # 增量索引
  rag index build --full --force           # 强制全量重建
```

```bash
# 查看索引状态
rag index status [选项]

选项:
  --verbose, -v        # 显示详细信息
  --json               # JSON 格式输出

示例:
  rag index status
  rag index status --json
```

```bash
# 清空索引
rag index clear [选项]

选项:
  --confirm            # 确认清空 (需要同时指定)
  --keep-config        # 保留配置文件

示例:
  rag index clear --confirm
```

```bash
# 导出/导入索引
rag index export --output <file>
rag index import --input <file>
```

#### 1.3.2 query - 问答查询

```bash
# 交互式问答
rag query [问题]

示例:
  rag query "RAG 系统如何构建索引？"
  rag query                                     # 进入交互模式

# 交互模式命令:
#   quit, exit, q       - 退出
#   history, h           - 显示历史
#   clear                - 清屏
#   help                 - 显示帮助
```

```bash
# 非交互式单次查询
rag query "问题" [--top-k <n>] [--no-context] [--stream]

选项:
  --top-k <n>           # 返回的最相关 Chunk 数 (默认: 5)
  --no-context          # 只返回相关 Chunk，不生成答案
  --stream              # 流式输出答案
  --output <file>       # 输出到文件

示例:
  rag query "解释一下快速排序算法" --top-k 3
  rag query "查找二叉树的实现" --no-context
  rag query "什么是向量索引" --stream
```

#### 1.3.3 document - 文档管理

```bash
# 列出已索引的文档
rag doc list [选项]

选项:
  --type <type>         # 按类型过滤 (md/cpp/pdf/...)
  --limit <n>           # 返回数量限制 (默认: 100)
  --offset <n>          # 分页偏移
  --json                # JSON 格式输出

示例:
  rag doc list
  rag doc list --type cpp --limit 50
  rag doc list --json | jq '.'
```

```bash
# 查看文档详情
rag doc show <document_id>

选项:
  --chunks              # 显示 Chunk 列表
  --content             # 显示文档内容
  --metadata            # 显示元数据

示例:
  rag doc show doc_abc123
  rag doc show doc_abc123 --chunks
```

```bash
# 删除文档
rag doc delete <document_id> [--confirm]

选项:
  --confirm             # 确认删除
  --recursive           # 删除包含的文件

示例:
  rag doc delete doc_abc123 --confirm
```

```bash
# 重新索引文档
rag doc reindex <document_id|pattern>
```

#### 1.3.4 config - 配置管理

```bash
# 查看当前配置
rag config show [--json]

# 编辑配置
rag config edit [--editor <editor>]

# 验证配置
rag config validate

# 重置为默认配置
rag config reset [--confirm]
```

#### 1.3.5 model - 模型管理

```bash
# 查看模型状态
rag model status

# 下载/更新模型
rag model download --model <name>

# 清理模型缓存
rag model cache clean
```

#### 1.3.6 辅助命令

```bash
# 显示版本
rag version
rag --version

# 显示帮助
rag help [command]
rag --help
```

---

## 2. REST API 设计

### 2.1 服务配置

```yaml
server:
  host: "127.0.0.1"
  port: 8080
  workers: 4
  timeout: 300          # 请求超时 (秒)
  cors:
    enabled: true
    origins: ["*"]
  auth:
    enabled: false      # 是否启用认证
    token_header: "X-API-Token"
```

### 2.2 API 端点概览

| 方法 | 端点 | 描述 |
|------|------|------|
| GET | `/health` | 健康检查 |
| GET | `/api/v1/status` | 索引状态 |
| POST | `/api/v1/index/build` | 构建索引 |
| POST | `/api/v1/index/clear` | 清空索引 |
| GET | `/api/v1/documents` | 文档列表 |
| GET | `/api/v1/documents/{id}` | 文档详情 |
| DELETE | `/api/v1/documents/{id}` | 删除文档 |
| POST | `/api/v1/query` | 问答查询 |
| POST | `/api/v1/query/chunks` | 仅检索 Chunk |
| GET | `/api/v1/history` | 查询历史 |
| GET | `/api/v1/config` | 获取配置 |
| PUT | `/api/v1/config` | 更新配置 |

### 2.3 详细 API 定义

#### 2.3.1 健康检查

```
GET /health
```

**响应：**
```json
{
  "status": "healthy",
  "version": "1.0.0",
  "uptime": 3600,
  "models": {
    "embedding": "loaded",
    "llm": "loaded"
  },
  "index": {
    "documents": 150,
    "chunks": 3200
  }
}
```

#### 2.3.2 获取索引状态

```
GET /api/v1/status
```

**响应：**
```json
{
  "index": {
    "name": "default",
    "status": "ready",
    "total_documents": 150,
    "total_chunks": 3200,
    "last_updated": "2026-07-06T10:30:00Z"
  },
  "models": {
    "embedding": {
      "name": "bge-base-zh-v1.5",
      "dimension": 768,
      "status": "ready"
    },
    "llm": {
      "name": "Qwen2.5-7B",
      "status": "ready"
    }
  }
}
```

#### 2.3.3 构建索引

```
POST /api/v1/index/build
Content-Type: application/json

{
  "paths": ["./docs", "./src"],
  "patterns": ["**/*.md", "**/*.cpp"],
  "incremental": true,
  "workers": 4,
  "batch_size": 32
}
```

**响应：**
```json
{
  "task_id": "task_abc123",
  "status": "started",
  "estimated_duration": 120
}
```

**查询任务状态：**
```
GET /api/v1/index/build/{task_id}
```

**响应：**
```json
{
  "task_id": "task_abc123",
  "status": "running",
  "progress": {
    "total": 150,
    "completed": 75,
    "current_file": "src/algorithm/sort.cpp"
  },
  "started_at": "2026-07-06T10:00:00Z"
}
```

#### 2.3.4 清空索引

```
POST /api/v1/index/clear
Content-Type: application/json

{
  "confirm": true,
  "keep_config": true
}
```

#### 2.3.5 文档列表

```
GET /api/v1/documents
GET /api/v1/documents?type=md&limit=20&offset=0
```

**响应：**
```json
{
  "total": 150,
  "limit": 20,
  "offset": 0,
  "documents": [
    {
      "id": "doc_a1b2c3",
      "file_path": "docs/notes/cpp.md",
      "file_name": "cpp.md",
      "file_type": "md",
      "file_size": 10240,
      "title": "C++ 学习笔记",
      "indexed_at": "2026-07-06T10:30:00Z"
    }
  ]
}
```

#### 2.3.6 文档详情

```
GET /api/v1/documents/{id}
```

**响应：**
```json
{
  "id": "doc_a1b2c3",
  "file_path": "docs/notes/cpp.md",
  "file_name": "cpp.md",
  "file_type": "md",
  "file_size": 10240,
  "file_hash": "abc123...",
  "title": "C++ 学习笔记",
  "summary": "本文介绍 C++ 的核心特性...",
  "metadata": {
    "headings": ["简介", "语法", "STL"],
    "language": "zh-CN"
  },
  "chunks": [
    {
      "id": "chunk_x9y8z7",
      "chunk_index": 0,
      "content_length": 500,
      "start_line": 1,
      "end_line": 15
    }
  ],
  "indexed_at": "2026-07-06T10:30:00Z"
}
```

#### 2.3.7 删除文档

```
DELETE /api/v1/documents/{id}
```

**响应：**
```json
{
  "id": "doc_a1b2c3",
  "status": "deleted"
}
```

#### 2.3.8 问答查询

```
POST /api/v1/query
Content-Type: application/json

{
  "question": "RAG 系统的索引构建流程是什么？",
  "top_k": 5,
  "stream": false,
  "include_metadata": true
}
```

**响应（非流式）：**
```json
{
  "answer": "RAG 系统的索引构建流程包括以下步骤...",
  "question": "RAG 系统的索引构建流程是什么？",
  "chunks": [
    {
      "id": "chunk_001",
      "document_id": "doc_a1b2c3",
      "content": "RAG 系统构建索引时，首先...",
      "score": 0.92,
      "metadata": {
        "file_path": "docs/rag.md",
        "start_line": 10
      }
    }
  ],
  "latencies": {
    "embedding": 45,
    "search": 12,
    "generation": 2300,
    "total": 2357
  },
  "created_at": "2026-07-06T10:35:00Z"
}
```

**响应（流式）：**
```
POST /api/v1/query
Content-Type: application/json

{
  "question": "解释快速排序",
  "stream": true
}
```

流式响应使用 Server-Sent Events (SSE)：
```
data: {"type": "chunk", "content": "快速排序是"}
data: {"type": "chunk", "content": "一种高效的"}
data: {"type": "chunk", "content": "排序算法，"}
data: {"type": "done"}
data: {"type": "chunks", "chunks": [...]}
data: {"type": "latencies", "values": {...}}
```

#### 2.3.9 仅检索 Chunk

```
POST /api/v1/query/chunks
Content-Type: application/json

{
  "question": "HNSW 索引的参数",
  "top_k": 10,
  "filters": {
    "file_type": "md"
  }
}
```

**响应：**
```json
{
  "chunks": [
    {
      "id": "chunk_001",
      "document_id": "doc_a1b2c3",
      "content": "HNSW 是一种基于图的向量索引...",
      "score": 0.95,
      "metadata": {
        "file_path": "docs/index.md",
        "start_line": 50
      }
    }
  ],
  "total": 10,
  "latencies": {
    "embedding": 45,
    "search": 12,
    "total": 57
  }
}
```

#### 2.3.10 查询历史

```
GET /api/v1/history
GET /api/v1/history?limit=20&offset=0
```

**响应：**
```json
{
  "total": 50,
  "limit": 20,
  "offset": 0,
  "history": [
    {
      "id": "query_xyz789",
      "query_text": "RAG 系统的索引构建流程是什么？",
      "answer": "RAG 系统构建索引时，首先...",
      "created_at": "2026-07-06T10:35:00Z"
    }
  ]
}
```

#### 2.3.11 配置管理

```
GET /api/v1/config
```

**响应：**
```json
{
  "data": {
    "sources": [...],
    "storage": {...}
  },
  "embedding": {
    "model_name": "bge-base-zh-v1.5",
    "dimension": 768,
    "batch_size": 32
  },
  "llm": {
    "model_name": "Qwen2.5-7B",
    "temperature": 0.7,
    "max_tokens": 2048
  },
  "retrieval": {
    "top_k": 5,
    "hnsw_weight": 0.6,
    "bm25_weight": 0.4
  }
}
```

```
PUT /api/v1/config
Content-Type: application/json

{
  "retrieval": {
    "top_k": 10
  }
}
```

### 2.4 错误响应

所有错误响应遵循统一格式：

```json
{
  "error": {
    "code": "INDEX_NOT_READY",
    "message": "索引尚未构建完成",
    "details": {
      "status": "building",
      "progress": "50%"
    }
  },
  "request_id": "req_abc123"
}
```

### 2.5 错误码定义

| 错误码 | HTTP 状态码 | 说明 |
|--------|-------------|------|
| `INVALID_REQUEST` | 400 | 请求参数无效 |
| `UNAUTHORIZED` | 401 | 未授权 |
| `NOT_FOUND` | 404 | 资源不存在 |
| `INDEX_NOT_READY` | 503 | 索引未就绪 |
| `MODEL_NOT_LOADED` | 503 | 模型未加载 |
| `INTERNAL_ERROR` | 500 | 内部错误 |

---

## 3. API 使用示例

### 3.1 curl 示例

```bash
# 健康检查
curl http://localhost:8080/health

# 构建索引
curl -X POST http://localhost:8080/api/v1/index/build \
  -H "Content-Type: application/json" \
  -d '{"paths": ["./docs"], "incremental": true}'

# 问答查询
curl -X POST http://localhost:8080/api/v1/query \
  -H "Content-Type: application/json" \
  -d '{"question": "RAG 系统的架构是什么？"}'

# 流式查询
curl -X POST http://localhost:8080/api/v1/query \
  -H "Content-Type: application/json" \
  -d '{"question": "解释快速排序", "stream": true}'
```

### 3.2 Python SDK 示例

```python
import requests

# 基础用法
api = "http://localhost:8080"

# 问答查询
response = requests.post(f"{api}/api/v1/query", json={
    "question": "RAG 系统如何构建索引？",
    "top_k": 5
})
result = response.json()
print(result["answer"])

# 流式查询
response = requests.post(f"{api}/api/v1/query", json={
    "question": "解释快速排序",
    "stream": True
}, stream=True)
for line in response.iter_lines():
    if line.startswith("data: "):
        data = json.loads(line[6:])
        if data["type"] == "chunk":
            print(data["content"], end="")
        elif data["type"] == "done":
            print()
```

---

## 4. API 端点汇总表

| 方法 | 路径 | 描述 | 认证 |
|------|------|------|------|
| GET | `/health` | 健康检查 | 否 |
| GET | `/api/v1/status` | 系统状态 | 否 |
| POST | `/api/v1/index/build` | 构建索引 | 可选 |
| POST | `/api/v1/index/clear` | 清空索引 | 可选 |
| GET | `/api/v1/documents` | 文档列表 | 否 |
| GET | `/api/v1/documents/{id}` | 文档详情 | 否 |
| DELETE | `/api/v1/documents/{id}` | 删除文档 | 可选 |
| POST | `/api/v1/query` | 问答查询 | 否 |
| POST | `/api/v1/query/chunks` | 检索 Chunk | 否 |
| GET | `/api/v1/history` | 查询历史 | 否 |
| GET | `/api/v1/config` | 获取配置 | 可选 |
| PUT | `/api/v1/config` | 更新配置 | 可选 |

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
