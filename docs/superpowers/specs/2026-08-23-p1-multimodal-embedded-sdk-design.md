# P1 多模态嵌入式 SDK 设计文档

## 1. 目标与边界

### 1.1 一句话目标

**将自实现数据库的多模态存储能力封装为嵌入式 SDK，对标 Chroma / lancedb / DuckDB，支持 Python + C + C++ + Go 四语言绑定，面向 v1.0 发布。**

### 1.2 MVP 范围（P1）

| 做 | 不做 |
|---|---|
| Collection + 4 模型（向量/图/时序/文本） | SQL 解析器封装（留给 P4） |
| C ABI 为锚 + pybind11 / cgo / RAII | 服务端模式（Docker、多租户） |
| SQLite 作为嵌入式后端 | 分布式存储 / Sharding |
| 常见 metadata 过滤语法 | JSONPath（留给 P3） |
| 跨模型 join（推迟到 P2） | RAG pipeline、embedding 适配器 |
| v1.0 LTS 路径（12+ 个月） | JIT / 向量化执行（内核已有） |

### 1.3 发布节奏

```
P1（本 spec）→ P2 持久化层 → P3 查询引擎 → P4 SQL 层
→ v1.0-beta → v1.0-rc → v1.0-LTS
```

---

## 2. 架构

### 2.1 分层架构

```
┌─────────────────────────────────────────────────────────┐
│                  SDK 多语言绑定层                          │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────┐│
│  │  Python   │ │  C / C++ │ │   Go     │ │  （预留其他） ││
│  │ pybind11  │ │ C ABI +  │ │  cgo +   │ │              ││
│  │ + numpy   │ │ RAII     │ │ 包级 API  │ │              ││
│  └──────────┘ └──────────┘ └──────────┘ └──────────────┘│
└──────────────────────┬──────────────────────────────────┘
                       │ FFI 调用
┌──────────────────────▼──────────────────────────────────┐
│                    C API 层                               │
│  mmdb_open / close / collection_create / drop            │
│  mmdb_vectors_add / search / delete / get                │
│  mmdb_graph_add_node / add_edge / query_path             │
│  mmdb_timeseries_append / query / downsample             │
│  mmdb_text_add / search                                  │
│  错误处理：int 错误码 + last_error() + auto-close        │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│                 业务逻辑层（C 实现）                        │
│  Collection 管理 · 数据模型 · Metadata 过滤 · 跨模型调度   │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│                  SQLite 嵌入层                             │
│  4 张核心表：vectors / graph / timeseries / text           │
│  SQLite JSON 扩展（metadata 过滤）                        │
│  FTS5（文本全文检索）                                      │
│  WAL 模式（并发安全）                                      │
└──────────────────────┬──────────────────────────────────┘
                       │
┌──────────────────────▼──────────────────────────────────┐
│                 磁盘文件层                                 │
│           <path>/multimodal.db（单文件嵌入式）              │
└─────────────────────────────────────────────────────────┘
```

### 2.2 与现有代码的关系

- **现有 `engineering/src/db/`**：保持不动，继续用于工程级 OLTP 和内核研究
- **P1 新增 `engineering/src/sdk/`**：独立模块，使用 SQLite（vendored）作为存储后端
- **共享**：项目基础设施（CMake、gtest、CI）复用现有框架

---

## 3. 模块划分

### 3.1 目录结构

```
engineering/
├── src/sdk/                    # SDK 核心实现（C）
│   ├── CMakeLists.txt
│   ├── core/                   # 嵌入式 DB 生命周期、Collection CRUD
│   │   ├── mmdb.c              # mmdb_open / close / last_error
│   │   ├── collection.c        # collection create / get / drop
│   │   ├── schema.c            # schema 定义与验证
│   │   ├── sqlite_backend.c    # SQLite 后端封装
│   │   ├── filter_parser.c     # metadata 过滤解析器
│   │   └── error.c             # 错误码 + 错误消息
│   ├── vectors/                # 向量模型
│   │   ├── vectors.c           # add / upsert / delete / search / get
│   │   ├── index_flat.c        # 暴力搜索
│   │   ├── index_ivf.c         # IVF-Flat 索引
│   │   ├── index_hnsw.c        # HNSW 索引
│   │   └── vectors_sql.c       # SQL 查询构造
│   ├── graph/                  # 图模型
│   │   ├── graph.c             # add_node / add_edge / delete / query_path
│   │   ├── graph_traverse.c    # BFS/DFS/最短路径
│   │   └── graph_sql.c         # SQL 查询构造
│   ├── timeseries/             # 时序模型
│   │   ├── timeseries.c        # append / query / downsample
│   │   ├── agg.c               # 聚合函数（avg/sum/min/max/count）
│   │   └── timeseries_sql.c    # SQL 查询构造
│   ├── text/                   # 文本模型
│   │   ├── text.c              # add / search / delete
│   │   ├── text_fts5.c         # FTS5 封装
│   │   └── text_sql.c          # SQL 查询构造
│   └── extra/                  # 跨模型查询、RAG、未来槽位
│       ├── cross_model.c       # 跨模型 join / union
│       └── rag.c               # RAG pipeline（预留）
├── include/sdk/                # 公共头文件
│   ├── mmdb.h                  # 顶层入口
│   ├── mmdb_vectors.h          # 向量 API
│   ├── mmdb_graph.h            # 图 API
│   ├── mmdb_timeseries.h       # 时序 API
│   ├── mmdb_text.h             # 文本 API
│   ├── mmdb_types.h            # 公共类型定义
│   └── mmdb_error.h            # 错误码定义
├── include/sdk/impl/           # C++ RAII 封装
│   ├── mmdb_db.hpp             # mmdb::DB RAII 类
│   ├── mmdb_collection.hpp     # mmdb::Collection RAII 类
│   └── mmdb_result.hpp         # mmdb::Result RAII 类
├── sdk/python/                 # Python 绑定
│   ├── pyproject.toml
│   ├── setup.py
│   ├── pymultimodal/           # Python 包
│   │   ├── __init__.py
│   │   ├── _core.cpython-3XX.so  # pybind11 编译产物
│   │   ├── db.py               # DB 类
│   │   ├── collection.py       # Collection 类
│   │   └── types.py            # 类型提示
│   └── tests/
├── sdk/go/                     # Go 绑定
│   ├── go.mod
│   ├── mmdb.go                 # Go API
│   ├── mmdb_test.go
│   └── internal/               # cgo 绑定内部实现
└── test/sdk/                   # C/C++ 测试
    ├── mmdb_core_test.cpp
    ├── mmdb_vectors_test.cpp
    ├── mmdb_graph_test.cpp
    ├── mmdb_timeseries_test.cpp
    ├── mmdb_text_test.cpp
    └── mmdb_filter_test.cpp
```

### 3.2 模块职责

| 模块 | 文件 | 职责 | 依赖 |
|------|------|------|------|
| core | mmdb.c, collection.c, schema.c, sqlite_backend.c, filter_parser.c, error.c | 生命周期、资源管理、元数据查询 | SQLite |
| vectors | vectors.c, index_flat/ivf/hnsw.c, vectors_sql.c | 向量 CRUD + 搜索 | core |
| graph | graph.c, graph_traverse.c, graph_sql.c | 图 CRUD + 路径查询 | core |
| timeseries | timeseries.c, agg.c, timeseries_sql.c | 时序 append + 聚合查询 | core |
| text | text.c, text_fts5.c, text_sql.c | 文本 CRUD + 全文检索 | core |
| extra | cross_model.c, rag.c | 跨模型 join（P2） | core + 各模型 |

---

## 4. C API 表面

### 4.1 生命周期与错误处理

```c
// 错误码
typedef enum {
    MMDB_OK              =  0,
    MMDB_ERR_INVALID     = -1,   // 参数非法
    MMDB_ERR_NOT_FOUND   = -2,   // 资源不存在
    MMDB_ERR_ALREADY     = -3,   // 资源已存在
    MMDB_ERR_IO          = -4,   // 磁盘 I/O
    MMDB_ERR_CORRUPT     = -5,   // 数据损坏
    MMDB_ERR_FULL        = -6,   // 磁盘满
    MMDB_ERR_INTERNAL    = -7,   // 内部错误
    MMDB_ERR_NOMEM       = -8,   // 内存不足
    MMDB_ERR_TIMEOUT     = -9,   // 超时
    MMDB_ERR_BUSY        = -10,  // 资源忙
} mmdb_error_t;

// DB
mmdb_t* mmdb_open(const char* path, const mmdb_options_t* opts);
void    mmdb_close(mmdb_t* db);
int     mmdb_last_error_code(mmdb_t* db);
const char* mmdb_last_error_message(mmdb_t* db);

// Collection
mmdb_collection_t* mmdb_collection_get(mmdb_t* db, const char* name);
mmdb_collection_t* mmdb_collection_create(mmdb_t* db, const char* name,
                                           const mmdb_schema_t* schema);
void               mmdb_collection_drop(mmdb_collection_t* coll);
const char*        mmdb_collection_name(mmdb_collection_t* coll);
```

### 4.2 向量模型

```c
// 向量条目
typedef struct {
    const uint8_t*  id;           // 外部 ID（可选，NULL 由系统生成）
    size_t          id_len;
    const float*    vector;       // 嵌入向量
    size_t          dim;          // 维度
    const char*     metadata_json; // JSON metadata（可选）
    const char*     text;         // 关联文本（可选）
} mmdb_vector_t;

// 搜索条件
typedef struct {
    const float*    query_vector;
    size_t          dim;
    size_t          top_k;
    const char*     filter_json;  // metadata 过滤（JSON 对象）
} mmdb_query_t;

// 搜索结果
typedef struct {
    size_t          count;
    mmdb_result_item_t* items;    // 需 mmdb_result_free 释放
} mmdb_result_t;

int mmdb_vectors_add(mmdb_collection_t* c, const mmdb_vector_t* vecs, size_t n);
int mmdb_vectors_upsert(mmdb_collection_t* c, const mmdb_vector_t* vecs, size_t n);
int mmdb_vectors_search(mmdb_collection_t* c, const mmdb_query_t* q, mmdb_result_t* out);
int mmdb_vectors_get(mmdb_collection_t* c, const uint8_t* id, size_t id_len, mmdb_vector_t* out);
int mmdb_vectors_delete(mmdb_collection_t* c, const uint8_t* id, size_t id_len);
void mmdb_result_free(mmdb_result_t* result);
```

### 4.3 图模型

```c
typedef struct {
    const char* id;
    const char* label;        // 节点类型
    const char* properties_json;
} mmdb_node_t;

typedef struct {
    const char* source_id;
    const char* target_id;
    const char* label;        // 边类型
    double      weight;       // 权重（可选）
    const char* properties_json;
} mmdb_edge_t;

typedef struct {
    const char* node_id;
    const char* label;
    const char* properties_json;
} mmdb_path_node_t;

typedef struct {
    size_t              node_count;
    mmdb_path_node_t*   nodes;
    size_t              edge_count;
    mmdb_edge_t*        edges;   // 路径上的边
} mmdb_path_t;

int mmdb_graph_add_node(mmdb_collection_t* c, const mmdb_node_t* node);
int mmdb_graph_add_edge(mmdb_collection_t* c, const mmdb_edge_t* edge);
int mmdb_graph_delete_node(mmdb_collection_t* c, const char* node_id);
int mmdb_graph_delete_edge(mmdb_collection_t* c, const char* source_id, const char* target_id, const char* edge_label);
int mmdb_graph_shortest_path(mmdb_collection_t* c, const char* from_id, const char* to_id, mmdb_path_t* out);
int mmdb_graph_bfs(mmdb_collection_t* c, const char* start_id, size_t max_depth, mmdb_result_t* out);
int mmdb_graph_dfs(mmdb_collection_t* c, const char* start_id, size_t max_depth, mmdb_result_t* out);
void mmdb_path_free(mmdb_path_t* path);
```

### 4.4 时序模型

```c
typedef struct {
    int64_t     timestamp;     // Unix 时间戳（毫秒）
    double      value;
    const char* tags_json;     // 标签（JSON 对象）
} mmdb_datapoint_t;

typedef struct {
    int64_t     start;         // 起始时间（毫秒）
    int64_t     end;           // 结束时间（毫秒）
    const char* agg;           // 聚合函数："avg"/"sum"/"min"/"max"/"count"/NULL（原始数据）
    const char* filter_json;   // 标签过滤
} mmdb_ts_query_t;

int mmdb_timeseries_append(mmdb_collection_t* c, const mmdb_datapoint_t* dp);
int mmdb_timeseries_append_batch(mmdb_collection_t* c, const mmdb_datapoint_t* dps, size_t n);
int mmdb_timeseries_query(mmdb_collection_t* c, const mmdb_ts_query_t* q, mmdb_result_t* out);
```

### 4.5 文本模型

```c
typedef struct {
    const char* id;            // 外部 ID（可选）
    const char* text;          // 文本内容
    const char* metadata_json; // 关联元数据（可选）
} mmdb_text_entry_t;

typedef struct {
    const char* query;         // 搜索关键词
    size_t      top_k;
    const char* filter_json;   // metadata 过滤
} mmdb_text_query_t;

int mmdb_text_add(mmdb_collection_t* c, const mmdb_text_entry_t* entry);
int mmdb_text_add_batch(mmdb_collection_t* c, const mmdb_text_entry_t* entries, size_t n);
int mmdb_text_search(mmdb_collection_t* c, const mmdb_text_query_t* q, mmdb_result_t* out);
int mmdb_text_get(mmdb_collection_t* c, const char* id, mmdb_text_entry_t* out);
int mmdb_text_delete(mmdb_collection_t* c, const char* id);
```

---

## 5. 数据模型与 Schema

### 5.1 Collection Schema 定义

```c
typedef enum {
    MMDB_TYPE_VECTOR,
    MMDB_TYPE_NODE,
    MMDB_TYPE_EDGE,
    MMDB_TYPE_DATAPOINT,
    MMDB_TYPE_TEXT,
} mmdb_data_type_t;

typedef struct {
    const char*     name;       // 字段名
    mmdb_data_type_t type;      // 数据类型
    int             nullable;   // 是否可空
    const char*     default_value_json; // 默认值
} mmdb_field_def_t;

typedef struct {
    const char*     model;           // "vector" / "graph" / "timeseries" / "text"
    size_t          field_count;
    mmdb_field_def_t* fields;
    // 模型特有参数
    size_t          vector_dim;      // 向量维度（仅 vector 模型）
    // 图模型不需要额外参数
    // 时序模型：time_index = "timestamp"（固定）
    // 文本模型：text_index = "text"（FTS5 索引列名）
} mmdb_schema_t;
```

### 5.2 Metadata 过滤语法（JSON 对象）

```json
// 等值
{"status": "active"}

// 比较
{"score": {"$gt": 90, "$lte": 100}}

// 包含
{"tags": {"$in": ["important", "urgent"]}}

// 嵌套
{"user": {"age": {"$gte": 18}}}

// 组合
{"$and": [
    {"status": "active"},
    {"$or": [{"score": {"$gt": 90}}, {"priority": "high"}]}
]}
```

---

## 6. SQLite 存储方案

### 6.1 Schema

```sql
-- Collection 元数据
CREATE TABLE collections (
    id          INTEGER PRIMARY KEY,
    name        TEXT UNIQUE NOT NULL,
    schema_json TEXT NOT NULL,
    created_at  INTEGER NOT NULL DEFAULT (unixepoch('now')),
    updated_at  INTEGER NOT NULL DEFAULT (unixepoch('now'))
);

-- 向量表
CREATE TABLE vectors (
    collection_id INTEGER NOT NULL REFERENCES collections(id),
    id            BLOB PRIMARY KEY,            -- 外部 ID
    vector        BLOB NOT NULL,               -- float 数组（小端）
    dim           INTEGER NOT NULL,
    metadata_json TEXT,
    text          TEXT,
    created_at    INTEGER NOT NULL DEFAULT (unixepoch('now')),
    updated_at    INTEGER NOT NULL DEFAULT (unixepoch('now'))
);

-- 图节点表
CREATE TABLE graph_nodes (
    collection_id INTEGER NOT NULL REFERENCES collections(id),
    id            TEXT PRIMARY KEY,
    label         TEXT,
    properties_json TEXT,
    created_at    INTEGER NOT NULL DEFAULT (unixepoch('now'))
);

-- 图边表
CREATE TABLE graph_edges (
    collection_id INTEGER NOT NULL REFERENCES collections(id),
    source_id     TEXT NOT NULL,
    target_id     TEXT NOT NULL,
    label         TEXT,
    weight        REAL DEFAULT 1.0,
    properties_json TEXT,
    created_at    INTEGER NOT NULL DEFAULT (unixepoch('now')),
    PRIMARY KEY (collection_id, source_id, target_id, label),
    FOREIGN KEY (source_id) REFERENCES graph_nodes(id),
    FOREIGN KEY (target_id) REFERENCES graph_nodes(id)
);

-- 时序表
CREATE TABLE timeseries (
    collection_id INTEGER NOT NULL REFERENCES collections(id),
    timestamp     INTEGER NOT NULL,
    value         REAL NOT NULL,
    tags_json     TEXT,
    created_at    INTEGER NOT NULL DEFAULT (unixepoch('now')),
    PRIMARY KEY (collection_id, timestamp)
);

-- 文本表 + FTS5 全文检索
CREATE TABLE texts (
    collection_id INTEGER NOT NULL REFERENCES collections(id),
    id            TEXT PRIMARY KEY,
    text          TEXT NOT NULL,
    metadata_json TEXT,
    created_at    INTEGER NOT NULL DEFAULT (unixepoch('now'))
);

CREATE VIRTUAL TABLE texts_fts USING fts5(
    text,
    content=texts,
    content_rowid=rowid
);
```

### 6.2 索引

```sql
-- 向量搜索（IVF/HNSW 在内存中计算，不建索引到 SQLite）
CREATE INDEX idx_vectors_collection ON vectors(collection_id);

-- 图：按节点查边
CREATE INDEX idx_edges_source ON graph_edges(source_id);
CREATE INDEX idx_edges_target ON graph_edges(target_id);
CREATE INDEX idx_edges_collection ON graph_edges(collection_id);

-- 时序：范围查询
CREATE INDEX idx_ts_collection ON timeseries(collection_id, timestamp);

-- 文本：全文检索
CREATE INDEX idx_texts_collection ON texts(collection_id);
```

### 6.3 关键配置

```sql
PRAGMA journal_mode = WAL;      -- 并发安全
PRAGMA synchronous = NORMAL;    -- 写入性能
PRAGMA busy_timeout = 5000;     -- 锁等待超时
PRAGMA cache_size = -8000;      -- 8MB 页缓存
PRAGMA mmap_size = 268435456;   -- 256MB mmap
```

---

## 7. 语言绑定方案

### 7.1 C / C++（免费赠品）

- **C API**：所有 `mmdb_*` 函数，C11 编译
- **C++ RAII**：`mmdb::DB`、`mmdb::Collection`、`mmdb::Result` 包装句柄，析构自动关闭
- **CMake 安装**：`find_package(mmsdk REQUIRED)` + `target_link_libraries(my_app mmsdk::static mmsdk::sqlite3)`
- **示例**：`engineering/src/sdk/examples/basic_usage.c`

### 7.2 Python（pybind11）

```python
# 安装
pip install pymultimodal

# 使用
import pymultimodal as mm

db = mm.DB("mydb.db")
coll = db.collection("my_coll")

# 向量 CRUD
coll.vectors.add(ids=["a","b"], vectors=[[1,2,3],[4,5,6]], metadatas=[{"x":1},{"x":2}])
results = coll.vectors.search(query=[1,2,3], top_k=5, filter={"x":{"$gt":0}})

# 图 CRUD
coll.graph.add_node(id="n1", label="Person", properties={"name":"Alice"})
coll.graph.add_edge(source="n1", target="n2", label="knows", weight=1.0)
path = coll.graph.shortest_path("n1", "n3")

# 时序 CRUD
coll.timeseries.append(timestamp=1234567890, value=42.0, tags={"sensor":"temp1"})
agg = coll.timeseries.query(start=0, end=9999999999, agg="avg")

# 文本 CRUD
coll.text.add(id="doc1", text="这是一篇文档", metadata={"source":"web"})
hits = coll.text.search("关键词", top_k=5)
```

### 7.3 Go（cgo）

```go
package main

import (
    mm "github.com/yourorg/pymultimodal/go"
)

func main() {
    db, _ := mm.Open("mydb.db", nil)
    defer db.Close()

    coll, _ := db.Collection("my_coll")

    // 向量
    coll.Vectors.Add([]mm.Vector{
        {ID: "a", Vector: []float32{1,2,3}, Metadata: map[string]interface{}{"x": 1}},
    })
    results, _ := coll.Vectors.Search([]float32{1,2,3}, 5, nil)

    // 图
    coll.Graph.AddNode(mm.Node{ID: "n1", Label: "Person", Properties: map[string]interface{}{"name": "Alice"}})
    coll.Graph.AddEdge(mm.Edge{Source: "n1", Target: "n2", Label: "knows"})

    // 时序
    coll.Timeseries.Append(mm.DataPoint{Timestamp: 1234567890, Value: 42.0, Tags: map[string]interface{}{"sensor": "temp1"}})

    // 文本
    coll.Text.Add(mm.TextEntry{ID: "doc1", Text: "这是一篇文档", Metadata: map[string]interface{}{"source": "web"}})
    hits, _ := coll.Text.Search("关键词", 5, nil)
}
```

---

## 8. 测试计划

### 8.1 单元测试（C++/GTest）

| 测试文件 | 覆盖内容 |
|---------|---------|
| `mmdb_core_test.cpp` | mmdb_open/close、错误处理、collection CRUD、schema 验证 |
| `mmdb_vectors_test.cpp` | 向量 add/upsert/delete/search/get、批量操作、空集合 |
| `mmdb_graph_test.cpp` | 图节点/边 CRUD、最短路径、BFS/DFS、空图 |
| `mmdb_timeseries_test.cpp` | 时序 append/query（聚合）、空集合、时间范围边界 |
| `mmdb_text_test.cpp` | 文本 add/search/get/delete、FTS5 分词、中文搜索 |
| `mmdb_filter_test.cpp` | 过滤语法解析、JSONPath 子集、错误过滤表达式 |
| `mmdb_python_test.cpp` | Python API 集成测试（构建后 import 测试） |

### 8.2 性能基准

| 测试项 | 目标 |
|-------|------|
| 向量 insert（10K 批量） | < 500ms |
| 向量 search（1K 向量，top_k=10） | < 5ms |
| 向量 search（1M 向量，top_k=10） | < 50ms（暴力），< 10ms（IVF/HNSW） |
| 图 shortest_path（1K 节点） | < 20ms |
| 时序 aggregate（100K 点） | < 100ms |
| 文本 search（10K 文档） | < 20ms |

### 8.3 Python / Go 集成测试

- Python：`pytest` 跑 `sdk/python/tests/`，覆盖 CRUD + filter + 搜索
- Go：`go test` 跑 `sdk/go/`，覆盖 CRUD + filter + 搜索

---

## 9. 错误处理约定

### 9.1 错误码

```c
typedef enum {
    MMDB_OK              =  0,   // 成功
    MMDB_ERR_INVALID     = -1,   // 参数非法
    MMDB_ERR_NOT_FOUND   = -2,   // 资源不存在
    MMDB_ERR_ALREADY     = -3,   // 资源已存在
    MMDB_ERR_IO          = -4,   // 磁盘 I/O 错误
    MMDB_ERR_CORRUPT     = -5,   // 数据损坏
    MMDB_ERR_FULL        = -6,   // 磁盘满
    MMDB_ERR_INTERNAL    = -7,   // 内部错误
    MMDB_ERR_NOMEM       = -8,   // 内存不足
    MMDB_ERR_TIMEOUT     = -9,   // 超时
    MMDB_ERR_BUSY        = -10,  // 资源忙
} mmdb_error_t;
```

### 9.2 错误检查模式

```c
// C 模式
int rc = mmdb_vectors_add(coll, vecs, n);
if (rc != MMDB_OK) {
    fprintf(stderr, "add failed: %s (code=%d)\n", mmdb_last_error_message(db), rc);
    goto cleanup;
}

// C++ RAII 模式（RAII 类在析构时自动关闭句柄，异常安全）
try {
    mm::DB db("mydb.db");
    auto coll = db.collection("my_coll");
    coll.vectors().add(vecs);
} catch (const mm::Error& e) {
    std::cerr << e.what() << std::endl;
}
```

### 9.3 Auto-close 语义

- `mmdb_close()` 在失败时仍能调用（幂等），不会触发二次释放
- Python/Go 绑定支持 context manager / defer
- RAII 类在析构时自动关闭

---

## 10. 风险与待决事项

| # | 风险 | 等级 | 对策 |
|---|------|------|------|
| R1 | SQLite 后端性能不如裸内存向量库（如 faiss） | 中 | MVP 用暴力搜索；IVF/HNSW 内存索引在后台建；TPC-C 不是目标 |
| R2 | 向量表 BLOB 持久化：float→BLOB 转换开销 | 中 | 用 `memcpy` + 小端格式；读取时直接映射 |
| R3 | FTS5 中文分词依赖 ICU 扩展 | 低 | 默认用 simple tokenizer，ICU 作为可选编译开关 |
| R4 | Python/Go 跨平台编译 | 中 | CI 先跑 Linux；Windows/macOS 后续补；优先保证功能正确 |
| R5 | C ABI 稳定性：发布后不能随意改 | 高 | P1 阶段明确标注 "0.x 不稳定"；v1.0 时锁定 ABI |

### 待决事项

1. **具体向量索引**：P1 是用 `index_flat.c` 暴力搜索，还是同时实现 `index_ivf.c` 和 `index_hnsw.c`？（建议：先 flat，P2 再加 IVF/HNSW）
2. **Python 包名**：`pymultimodal` 是否合适？是否改为 `mmdb` / `multimodaldb` / `mmstore`？
3. **Go 模块路径**：`github.com/yourorg/pymultimodal/go` 用什么实际路径？
4. **许可证**：MIT / Apache 2.0 / 未定？

---

## 11. 下一步

写完 spec 后，转入 `writing-plans` 技能，生成 P1 的实现任务清单（含任务依赖、时间估计、验收标准）。
