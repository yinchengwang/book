# RAG 系统数据层设计

## 概述

本文档定义 RAG 系统的数据层设计，包括数据源配置、存储 Schema、索引格式等。

---

## 1. 数据源配置

### 1.1 支持的文件类型

| 类型 | 扩展名 | 解析器 | 优先级 |
|------|--------|--------|--------|
| Markdown | `.md`, `.markdown` | MarkdownParser | 高 |
| PDF | `.pdf` | PDFParser | 高 |
| Word | `.docx` | DocxParser | 中 |
| Excel | `.xlsx`, `.xls` | ExcelParser | 中 |
| 纯文本 | `.txt`, `.log` | TextParser | 中 |
| C/C++ 代码 | `.c`, `.cpp`, `.h`, `.hpp` | CodeParser | 高 |
| Python | `.py` | CodeParser | 高 |
| Java | `.java` | CodeParser | 中 |
| JSON | `.json` | JsonParser | 中 |
| YAML | `.yaml`, `.yml` | YamlParser | 中 |

### 1.2 数据源目录结构

```
项目根目录/
├── docs/                    # 文档目录
│   ├── notes/              # 学习笔记
│   ├── README.md           # 项目说明
│   └── 设计文档/
├── src/                    # 源代码目录
├── test/                   # 测试目录
├── interview/              # 面试题目录
└── leetcode/               # LeetCode 目录
```

### 1.3 数据源配置项

```yaml
# rag-config.yaml
data_sources:
  - path: "./docs"
    patterns:
      - "**/*.md"
      - "**/*.txt"
    exclude:
      - "**/node_modules/**"
      - "**/.git/**"
    priority: 1

  - path: "./src"
    patterns:
      - "**/*.c"
      - "**/*.cpp"
      - "**/*.h"
    exclude:
      - "**/build/**"
    priority: 2

  - path: "./notes"
    patterns:
      - "**/*.md"
    priority: 1

  - path: "./interview"
    patterns:
      - "**/*.md"
      - "**/*.cpp"
    priority: 1

  - path: "./leetcode"
    patterns:
      - "**/*.cpp"
      - "**/*.c"
    priority: 2
```

---

## 2. 存储 Schema 设计

### 2.1 数据库选型

使用 **SQLite** 存储元数据，支持以下表结构：

### 2.2 documents 表

存储文档基本信息。

```sql
CREATE TABLE documents (
    id          TEXT PRIMARY KEY,           -- 文档唯一ID (UUID)
    file_path   TEXT NOT NULL,             -- 相对路径
    file_name   TEXT NOT NULL,             -- 文件名
    file_type   TEXT NOT NULL,             -- 文件类型 (md/pdf/cpp/...)
    file_size   INTEGER NOT NULL,          -- 文件大小 (字节)
    file_hash   TEXT NOT NULL,             -- 文件内容Hash (SHA256)
    title       TEXT,                       -- 提取的标题
    summary     TEXT,                       -- 文档摘要
    metadata    TEXT,                       -- 额外元数据 (JSON)
    created_at  INTEGER NOT NULL,           -- 创建时间戳
    updated_at  INTEGER NOT NULL,           -- 更新时间戳
    indexed_at  INTEGER                     -- 索引时间戳
);

CREATE INDEX idx_docs_file_type ON documents(file_type);
CREATE INDEX idx_docs_file_hash ON documents(file_hash);
CREATE INDEX idx_docs_updated ON documents(updated_at);
```

### 2.3 chunks 表

存储文档分块信息。

```sql
CREATE TABLE chunks (
    id              TEXT PRIMARY KEY,       -- Chunk唯一ID (UUID)
    document_id     TEXT NOT NULL,          -- 所属文档ID
    chunk_index     INTEGER NOT NULL,       -- 块在文档中的序号
    content         TEXT NOT NULL,          -- 块文本内容
    content_length  INTEGER NOT NULL,       -- 内容长度
    metadata        TEXT,                    -- 块元数据 (JSON)
    -- 位置信息
    start_line      INTEGER,                -- 起始行号
    end_line        INTEGER,                -- 结束行号
    -- 嵌入信息
    embedding_id    INTEGER,                -- 向量库中的ID (可选)
    -- 时间戳
    created_at      INTEGER NOT NULL,

    FOREIGN KEY (document_id) REFERENCES documents(id) ON DELETE CASCADE
);

CREATE INDEX idx_chunks_doc ON chunks(document_id);
CREATE INDEX idx_chunks_embedding ON chunks(embedding_id);
```

### 2.4 embeddings 表

存储向量索引元数据。

```sql
CREATE TABLE embeddings (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    chunk_id        TEXT NOT NULL,          -- Chunk ID
    model_name      TEXT NOT NULL,          -- 模型名称
    dimension       INTEGER NOT NULL,       -- 向量维度
    vector_path     TEXT NOT NULL,          -- 向量文件路径
    created_at      INTEGER NOT NULL,       -- 创建时间

    FOREIGN KEY (chunk_id) REFERENCES chunks(id) ON DELETE CASCADE
);

CREATE INDEX idx_emb_chunk ON embeddings(chunk_id);
```

### 2.5 index_status 表

存储索引构建状态。

```sql
CREATE TABLE index_status (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    index_name      TEXT NOT NULL,          -- 索引名称
    status          TEXT NOT NULL,          -- 状态 (building/ready/error)
    total_docs      INTEGER DEFAULT 0,      -- 文档总数
    indexed_docs    INTEGER DEFAULT 0,      -- 已索引文档数
    total_chunks    INTEGER DEFAULT 0,      -- 总块数
    error_message   TEXT,                    -- 错误信息
    started_at      INTEGER,                -- 开始时间
    completed_at    INTEGER,                -- 完成时间
    created_at      INTEGER NOT NULL,
    updated_at      INTEGER NOT NULL
);

CREATE INDEX idx_status_name ON index_status(index_name);
```

### 2.6 query_history 表

存储查询历史。

```sql
CREATE TABLE query_history (
    id              TEXT PRIMARY KEY,       -- 查询ID
    query_text      TEXT NOT NULL,          -- 查询文本
    answer          TEXT,                    -- 返回答案
    top_chunks      TEXT,                    -- 使用的Chunk ID列表 (JSON)
    latencies       TEXT,                    -- 各阶段延迟 (JSON)
    created_at      INTEGER NOT NULL
);

CREATE INDEX idx_history_created ON query_history(created_at);
```

### 2.7 metadata JSON 结构

```json
{
    "source": {
        "file_path": "docs/notes/cpp.md",
        "file_type": "md",
        "language": "zh-CN",
        "encoding": "utf-8"
    },
    "document": {
        "title": "C++ 学习笔记",
        "author": "yincheng",
        "headings": ["简介", "语法", "STL"],
        "toc": ["# C++ 学习笔记", "## 简介", "## 语法"]
    },
    "chunk": {
        "type": "paragraph",
        "parent_heading": "简介",
        "code_blocks": 0,
        "images": 0,
        "links": 2
    }
}
```

---

## 3. 索引格式设计

### 3.1 HNSW 索引参数

```yaml
hnsw:
  # 向量维度 (bge-base-zh-v1.5 = 768)
  dimension: 768

  # M 参数: 每个节点的最大连接数
  # 较大值: 更高精度, 更多内存, 更快构建
  # 推荐范围: 16-64
  m: 32

  # efConstruction: 构建时动态列表大小
  # 较大值: 更高精度, 更慢构建
  # 推荐范围: 100-400
  ef_construction: 200

  # efSearch: 搜索时动态列表大小
  # 推荐范围: 50-1000
  ef_search: 100

  # 构建线程数
  num_threads: 4

  # 距离度量: l2 / cosine / ip (内积)
  metric: cosine
```

### 3.2 BM25 参数

```yaml
bm25:
  # 文档频率平滑参数
  k1: 1.5

  # 词频饱和参数
  b: 0.75

  # 分词器类型: standard / chinese / code
  tokenizer: chinese

  # 是否存储文档长度
  avg_doc_len: 1000
```

### 3.3 向量存储文件结构

```
rag_data/
├── index/
│   ├── hnsw/
│   │   └── vectors.bin       # HNSW 索引文件
│   ├── bm25/
│   │   └── index.bin         # BM25 索引文件
│   └── metadata/
│       └── chunks_meta.json   # Chunk ID 映射
├── documents/
│   └── {hash}.txt            # 文档内容缓存
└── rag.db                     # SQLite 元数据库
```

---

## 4. 文件命名规范

### 4.1 文档 ID 生成

```
文档ID = SHA256(file_path + file_hash)[0:16]

示例: doc_a1b2c3d4e5f6g7h8
```

### 4.2 Chunk ID 生成

```
ChunkID = SHA256(doc_id + chunk_index + content_hash)[0:16]

示例: chunk_x9y8z7w6v5u4
```

### 4.3 文件路径规范

```yaml
paths:
  # 数据根目录
  data_root: "./rag_data"

  # 各子目录
  index_dir: "./rag_data/index"
  docs_dir: "./rag_data/documents"
  cache_dir: "./rag_data/cache"

  # 数据库路径
  db_path: "./rag_data/rag.db"

  # 向量文件路径
  vector_path: "./rag_data/index/hnsw/vectors.bin"
  bm25_path: "./rag_data/index/bm25/index.bin"
```

---

## 5. 数据流设计

### 5.1 入库数据流

```
源文件
  │
  ▼
┌─────────────────┐
│  文件扫描        │ ──→ 提取 file_path, file_hash
└─────────────────┘
  │
  ▼
┌─────────────────┐
│  变更检测        │ ──→ 比较 file_hash, 跳过未变更文件
└─────────────────┘
  │
  ▼
┌─────────────────┐
│  文档解析        │ ──→ MarkdownParser → 提取文本、标题、代码块
└─────────────────┘
  │
  ▼
┌─────────────────┐
│  内容分块        │ ──→ RecursiveChunker → Chunk[]
└─────────────────┘
  │
  ▼
┌─────────────────┐
│  向量生成        │ ──→ EmbeddingService.encode() → float[768][]
└─────────────────┘
  │
  ▼
┌─────────────────┐
│  索引存储        │ ──→ HNSW.add() + BM25.add() + SQLite
└─────────────────┘
```

### 5.2 数据一致性保证

| 场景 | 保证机制 |
|------|----------|
| 新增文档 | 先写入 SQLite，再更新向量索引 |
| 删除文档 | 先删除向量索引记录，再删除 SQLite |
| 更新文档 | 标记旧记录为 deleted，插入新记录 |
| 批量入库 | 事务保证原子性 |

---

## 6. 配置模板

```yaml
# rag-config.yaml - 数据层配置部分

data:
  # 数据源
  sources:
    - path: "./docs"
      patterns: ["**/*.md", "**/*.txt"]
      priority: 1
    - path: "./src"
      patterns: ["**/*.c", "**/*.cpp", "**/*.h"]
      priority: 2
    - path: "./notes"
      patterns: ["**/*.md"]
      priority: 1

  # 存储路径
  storage:
    data_root: "./rag_data"
    db_name: "rag.db"

  # 索引配置
  index:
    hnsw:
      dimension: 768
      m: 32
      ef_construction: 200
      ef_search: 100
      metric: cosine
    bm25:
      k1: 1.5
      b: 0.75
      tokenizer: chinese

  # 增量索引
  incremental:
    enabled: true
    hash_file: "./rag_data/.file_hashes.json"
```

---

## 7. 性能考虑

### 7.1 批量处理参数

```yaml
batch:
  # 文档批次大小
  doc_batch_size: 100

  # 向量编码批次大小
  embedding_batch_size: 32

  # 数据库提交批次大小
  db_commit_size: 500
```

### 7.2 缓存策略

| 数据类型 | 缓存策略 | TTL |
|----------|----------|-----|
| 文件内容 | LRU, 1000 文件 | 24h |
| 文件 Hash | 持久化 | 永久 |
| 向量索引 | mmap | 永久 |
| 解析结果 | 临时文件 | 构建期间 |

---

*文档版本：1.0.0*
*最后更新：2026-07-06*
