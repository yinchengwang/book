# RAG 系统配置设计

## 概述

本文档定义 RAG 系统的配置文件结构 (`rag-config.yaml`)，涵盖所有配置项的详细说明。

---

## 1. 配置概览

### 1.1 完整配置结构

```yaml
# ============================================
# RAG 系统配置文件
# ============================================

# 版本（必填，用于配置兼容性检查）
version: "1.0.0"

# 系统配置
system:
  # 日志配置
  log:
    level: info                    # trace, debug, info, warn, error
    file: "./rag_data/logs/rag.log"
    max_size: 100MB                 # 日志文件最大大小
    max_backups: 5                  # 保留的备份文件数
    format: json                    # text, json
    color: false                    # 是否彩色输出

  # 数据目录
  data_root: "./rag_data"

  # 临时文件目录
  temp_dir: "./rag_data/tmp"

  # 并行配置
  workers:
    indexing: 4                     # 索引构建线程数
    embedding: 2                    # 向量编码线程数
    io: 8                           # IO 线程数

# ============================================
# 数据源配置
# ============================================
data_sources:
  # 数据源列表
  - name: "docs"                   # 数据源名称
    path: "./docs"                  # 相对或绝对路径
    enabled: true                   # 是否启用
    patterns:                       # 文件匹配模式
      - "**/*.md"
      - "**/*.txt"
    exclude:                        # 排除模式
      - "**/node_modules/**"
      - "**/.git/**"
      - "**/build/**"
    priority: 1                    # 优先级 (数字越小优先级越高)
    metadata:
      language: zh-CN

  - name: "src"
    path: "./src"
    enabled: true
    patterns:
      - "**/*.c"
      - "**/*.cpp"
      - "**/*.h"
      - "**/*.hpp"
    exclude:
      - "**/build/**"
      - "**/*.o"
    priority: 2

  - name: "notes"
    path: "./notes"
    enabled: true
    patterns:
      - "**/*.md"
    priority: 1

  - name: "interview"
    path: "./interview"
    enabled: true
    patterns:
      - "**/*.md"
      - "**/*.cpp"
    priority: 1

  - name: "leetcode"
    path: "./leetcode"
    enabled: true
    patterns:
      - "**/*.cpp"
      - "**/*.c"
      - "**/*.java"
      - "**/*.py"
    priority: 2

# ============================================
# 存储配置
# ============================================
storage:
  # 数据库配置
  database:
    type: sqlite                    # sqlite, postgres (未来支持)
    path: "./rag_data/rag.db"
    # 连接池配置 (未来支持)
    pool:
      min_size: 1
      max_size: 10
      timeout: 30

  # 文档存储
  documents:
    cache_enabled: true            # 是否缓存文档内容
    cache_size: 1000               # 缓存文档数量
    cache_ttl: 86400               # 缓存 TTL (秒)

  # 索引存储
  index:
    format: binary                 # binary, mmap
    compression: false              # 是否压缩

# ============================================
# 分块配置
# ============================================
chunking:
  # 默认分块策略
  default_strategy: recursive      # fixed, recursive, semantic, code_aware

  # 通用参数
  chunk_size: 512                   # 目标块大小 (字符数)
  chunk_overlap: 50                 # 块重叠大小
  min_chunk_size: 100               # 最小块大小 (小于此值会合并)
  max_chunk_size: 1024              # 最大块大小 (超过会分割)

  # 固定大小分块
  fixed:
    size: 512
    overlap: 50

  # 递归字符分块
  recursive:
    separators: ["\n\n", "\n", "。", "！", "？", ". ", "! ", "? ", " ", ""]
    keep_separator: end            # start, end, none

  # 语义分块 (需要模型支持)
  semantic:
    model: "bge-large-zh-v1.5"
    threshold: 0.8                 # 相似度阈值
    min_sentences: 2               # 每块最小句子数

  # 代码感知分块
  code_aware:
    # C/C++ 特定
    cpp:
      max_function_lines: 200     # 函数最大行数
      preserve_comments: true       # 保留注释
      preserve_docstrings: true     # 保留文档字符串
    # Python 特定
    python:
      max_function_lines: 100
      preserve_docstrings: true

  # 按文件类型的分块策略覆盖
  type_overrides:
    cpp:
      strategy: code_aware
      chunk_size: 768
    py:
      strategy: code_aware
      chunk_size: 512
    md:
      strategy: recursive
      chunk_size: 768

# ============================================
# Embedding 配置
# ============================================
embedding:
  # 模型配置
  model:
    # 本地模型路径或 HuggingFace 模型名
    name: "bge-base-zh-v1.5"
    path: "./models/bge-base-zh-v1.5"
    type: sentence_transformers     # sentence_transformers, openai, local

    # 向量维度 (自动从模型推断)
    dimension: 768

    # 标准化
    normalize: true

    # 批处理
    batch_size: 32
    max_batch_tokens: 4096         # 每批最大 token 数

    # 设备配置
    device: auto                    # cpu, cuda, cuda:0, mps, auto
    num_threads: 4

  # 缓存配置
  cache:
    enabled: true
    path: "./rag_data/cache/embeddings"
    max_size: 10GB

# ============================================
# LLM 配置
# ============================================
llm:
  # 模型配置
  model:
    # 本地模型 (llama.cpp 格式)
    name: "Qwen2.5-7B"
    path: "./models/Qwen2.5-7B-Q4_K_M.gguf"

    # 量化配置
    quantization: Q4_K_M            # Q4_0, Q4_K_M, Q5_K_M, Q8_0, f16

    # llama.cpp 配置
    llama:
      context_size: 8192            # 上下文窗口大小
      gpu_layers: 35                # 加载到 GPU 的层数 (0 = 全 CPU)
      threads: 4                    # CPU 线程数
      batch_size: 512               # 批处理大小
      low_vram: false               # 低显存模式

    # API 模式配置 (可选)
    api:
      enabled: false
      base_url: "http://localhost:8080"
      api_key: ""
      model: "qwen2.5-7b"

  # 生成参数
  generation:
    temperature: 0.7                # 采样温度 (0.0 - 2.0)
    top_p: 0.9                      # Nucleus 采样
    top_k: 50                       # Top-K 采样
    repeat_penalty: 1.1              # 重复惩罚
    max_tokens: 2048                # 最大生成长度
    stop: []                        # 停止词列表

    # 流式输出
    stream: true

  # 缓存配置
  cache:
    enabled: true
    path: "./rag_data/cache/llm"
    max_size: 1GB

# ============================================
# 向量索引配置
# ============================================
vector_index:
  # HNSW 配置
  hnsw:
    dimension: 768                  # 向量维度
    m: 32                           # 连接数 (16-64)
    ef_construction: 200            # 构建时搜索范围 (100-400)
    ef_search: 100                  # 搜索时搜索范围 (50-1000)
    num_threads: 4                  # 构建线程数
    metric: cosine                  # 距离度量: l2, cosine, ip
    store_json: false               # 是否存储原始向量

  # 路径配置
  path: "./rag_data/index/hnsw"

# ============================================
# BM25 配置
# ============================================
bm25:
  # BM25 参数
  k1: 1.5                          # 词频饱和参数
  b: 0.75                          # 文档长度归一化参数

  # 分词器配置
  tokenizer:
    type: chinese                   # standard, chinese, code, whitespace
    # 中文分词特定配置
    chinese:
      mode: "index"                 # "index" 或 "search"
      hmm: true                     # 启用 HMM
      dictionary: ""                # 自定义词典路径
    # 代码分词配置
    code:
      split_on_punctuation: true
      split_on_numbers: true
      keep_identifiers: true

  # 路径配置
  path: "./rag_data/index/bm25"

# ============================================
# 检索配置
# ============================================
retrieval:
  # 检索参数
  top_k: 5                          # 返回的 Top-K 结果
  min_score: 0.3                    # 最低相关度阈值

  # 混合检索权重
  hybrid:
    hnsw_weight: 0.6                # HNSW 权重 (0.0 - 1.0)
    bm25_weight: 0.4                # BM25 权重 (0.0 - 1.0)

  # RRF 融合参数
  rrf:
    k: 60                           # RRF K 值 (通常 60)

  # 重排序配置
  rerank:
    enabled: false                  # 是否启用重排序
    model: ""                       # 重排序模型路径
    top_n: 10                       # 重排序候选数量

  # 过滤配置
  filter:
    enabled: true
    default_file_types: []          # 默认文件类型过滤 (空 = 不过滤)
    max_file_size: 100MB            # 最大文件大小

# ============================================
# 增量索引配置
# ============================================
incremental_indexing:
  enabled: true
  hash_file: "./rag_data/.file_hashes.json"

  # 文件变化检测
  detection:
    check_modified: true           # 检测修改
    check_deleted: true             # 检测删除
    hash_algorithm: sha256          # hash 算法

# ============================================
# Prompt 模板配置
# ============================================
prompt:
  # 系统提示词
  system: |
    你是一个专业的技术助手，基于提供的上下文信息回答用户的问题。
    请遵循以下规则：
    1. 只使用提供的上下文信息进行回答
    2. 如果上下文中没有相关信息，请明确说明"我无法从提供的文档中找到答案"
    3. 回答要准确、简洁、有条理
    4. 对于代码相关问题，请标注代码语言

  # 用户提示词模板
  user_template: |
    ## 上下文信息
    {{context}}

    ## 用户问题
    {{question}}

    请基于上述上下文信息回答问题。

  # 无上下文时的回复模板
  no_context_template: |
    对不起，我无法从当前索引的文档中找到与您问题相关的信息。
    您可以尝试：
    1. 扩大索引范围
    2. 调整检索参数
    3. 重新构建索引

# ============================================
# 服务配置 (REST API)
# ============================================
server:
  enabled: false                   # 是否启用 REST API

  # 网络配置
  host: "127.0.0.1"
  port: 8080
  workers: 4
  timeout: 300                      # 请求超时 (秒)

  # CORS 配置
  cors:
    enabled: true
    origins:
      - "http://localhost:3000"
      - "http://localhost:8080"

  # 认证配置
  auth:
    enabled: false
    token: ""                       # API Token

# ============================================
# 可观测性配置
# ============================================
observability:
  # 指标收集
  metrics:
    enabled: true
    port: 9090                      # Prometheus 指标端口
    path: "/metrics"

  # 追踪
  tracing:
    enabled: false
    endpoint: "http://localhost:4318"
    sample_rate: 1.0

  # 健康检查
  health:
    check_interval: 30              # 检查间隔 (秒)
    endpoints:
      - "127.0.0.1:8080"
      - "./models"

# ============================================
# 批处理配置
# ============================================
batch:
  # 文档批次大小
  doc_batch_size: 100

  # 向量编码批次大小
  embedding_batch_size: 32

  # 数据库提交批次大小
  db_commit_size: 500

  # 向量索引批次大小
  index_batch_size: 1000

# ============================================
# 错误处理配置
# ============================================
error_handling:
  # 重试配置
  retry:
    max_attempts: 3                 # 最大重试次数
    initial_delay: 1000             # 初始延迟 (毫秒)
    max_delay: 10000                # 最大延迟
    backoff: exponential             # linear, exponential

  # 降级策略
  fallback:
    on_embedding_error: skip       # skip, use_dummy, use_bm25_only
    on_llm_error: return_chunks     # return_chunks, return_error

  # 日志记录
  log_errors: true
  log_level: warn
```

---

## 2. 配置分组

### 2.1 必需配置

以下配置项是运行 RAG 系统所必需的：

```yaml
version: "1.0.0"                    # 必须指定版本
system.data_root: "./rag_data"      # 必须指定数据目录
data_sources: [...]                 # 必须指定至少一个数据源
embedding.model.path: "..."         # 必须指定 Embedding 模型路径
llm.model.path: "..."               # 必须指定 LLM 模型路径
```

### 2.2 可选配置

有默认值的可选配置：

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| `chunking.chunk_size` | 512 | 分块大小 |
| `chunking.chunk_overlap` | 50 | 块重叠 |
| `retrieval.top_k` | 5 | 检索结果数 |
| `llm.generation.temperature` | 0.7 | LLM 温度 |
| `server.port` | 8080 | API 端口 |

---

## 3. 环境变量覆盖

支持通过环境变量覆盖配置：

```bash
# 格式: RAG_{SECTION}_{KEY} (全大写，下划线分隔)

RAG_SYSTEM_DATA_ROOT="/path/to/data"
RAG_EMBEDDING_MODEL_PATH="/models/embedding"
RAG_LLM_MODEL_PATH="/models/llm.gguf"
RAG_SERVER_PORT=9000
RAG_LOG_LEVEL=debug
```

---

## 4. 配置验证

### 4.1 验证规则

```yaml
validation:
  # 模型路径必须存在
  - rule: model_path_exists
    fields: [embedding.model.path, llm.model.path]

  # 数据源路径必须存在
  - rule: source_path_exists
    fields: [data_sources[*].path]

  # 数值范围检查
  - rule: range_check
    field: chunking.chunk_size
    min: 100
    max: 4096

  - rule: range_check
    field: llm.generation.temperature
    min: 0.0
    max: 2.0

  # 权重和必须为 1
  - rule: weight_sum
    fields: [retrieval.hybrid.hnsw_weight, retrieval.hybrid.bm25_weight]
    sum: 1.0
```

### 4.2 配置模板生成

```bash
# 生成默认配置
rag config generate > rag-config.yaml

# 验证配置
rag config validate -c rag-config.yaml

# 查看配置差异
rag config diff -c rag-config.yaml
```

---

## 5. 快速开始配置

对于大多数用例，可以使用简化配置：

```yaml
# rag-config.yaml - 简化配置
version: "1.0.0"
system:
  data_root: "./rag_data"

data_sources:
  - path: "./docs"
    patterns: ["**/*.md"]
  - path: "./src"
    patterns: ["**/*.cpp", "**/*.h"]

embedding:
  model:
    path: "./models/bge-base-zh-v1.5"

llm:
  model:
    path: "./models/Qwen2.5-7B-Q4_K_M.gguf"

# 其余配置使用默认值
```

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
