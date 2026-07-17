# 工程代码对照笔记

## 对照源码

本示例展示 SQLite VSS 扩展概念。SQLite VSS 本身不存在于本工程中，
但其概念对应以下工程实现：

当前工程中向量相关实现位于：
- `engineering/src/db/api/vector_api.c` - 向量 API 实现
- `engineering/src/db/storage/vector/` - 向量存储引擎
- `engineering/src/db/index/vector_index/` - 向量索引（HNSW/IVF）

## SQLite VSS 扩展机制

SQLite VSS 通过 SQLite 扩展加载机制实现：

```c
// SQLite 扩展入口函数
int sqlite3_extension_init(
    sqlite3 *db,
    void **pzErrMsg,
    const sqlite3_api_routines *pApi
);
```

与工程代码对照：

| SQLite VSS 概念 | 工程实现对应 |
|----------------|-------------|
| 扩展函数注册 | `vector_api.c` 的 API 接口层 |
| 虚拟表 | `vector_engine.c` 的向量引擎 |
| 向量索引 | `vector_index/` 的 HNSW/IVF 实现 |
| 距离计算 | `vector_query.c` 的距离度量函数 |

## 核心数据结构对比

### SQLite VSS 虚拟表结构

```sql
-- 虚拟表定义
CREATE VIRTUAL TABLE vss_embedding USING vss(
    embedding(384)           -- 384 维向量
    params('hnsw', 'm=16')  -- HNSW 参数
);
```

### 工程版向量集合结构

`vector_api.c` 中的 `VectorCollectionImpl`：

```c
typedef struct VectorCollectionImpl_s {
    char name[128];              // 集合名称
    int32_t dimension;           // 向量维度
    int32_t size;                // 当前向量数量
    VectorIndexType index_type;  // 索引类型（HNSW/IVF）
    VectorMetricType metric_type; // 距离度量
    int64_t *ids;                // ID 数组
    float *vectors;              // 扁平化向量数据
} VectorCollectionImpl;
```

两者都采用扁平化向量数组存储，但 SQLite VSS 使用 BLOB 序列化，工程版直接用 `float*`。

## 函数对照表

### 1. 初始化

| SQLite VSS | 工程实现 |
|-----------|---------|
| `load_extension()` | `vector_api_create()` |
| 创建虚拟表 | `vector_create_collection()` |

### 2. 向量操作

| SQLite VSS SQL | 工程实现 |
|---------------|---------|
| `INSERT INTO vss_embedding VALUES (...)` | `vector_insert()` |
| `SELECT * FROM vss_embedding WHERE vss_search(...)` | `vector_search()` |
| `DELETE FROM vss_embedding WHERE rowid = ?` | `vector_delete()` |

### 3. 距离计算

| SQLite VSS | 工程实现 |
|-----------|---------|
| `vss_euclidean_distance()` | 底层调用 `distance_l2sqr()` |
| `vss_cosine_distance()` | 底层调用 `distance_cosine()` |
| `vss_inner_product()` | 底层调用 `distance_inner_product()` |

## 索引实现差异

SQLite VSS 的 HNSW/IVF 实现：
- 作为 SQLite 虚拟表模块实现
- 使用 C++ 编写的 VAMANA 图算法
- 通过 xBestIndex/xFilter 回调与 SQLite 集成

工程实现（`vector_index/`）：
- 纯 C 实现
- 模块化设计，支持多种索引类型
- 与 Buffer Pool / WAL 深度集成

## 实现建议

如需在工程中实现 `sqlite_vss.c`，可参考：

1. **入口函数**：`sqlite3_extension_init` 注册函数和虚拟表
2. **虚拟表模块**：实现 `sqlite3_module` 接口（xCreate/xConnect/xBestIndex 等）
3. **向量存储**：使用工程现有的 `vector_engine.c` 存储逻辑
4. **索引集成**：复用 `vector_index/` 的 HNSW/IVF 实现
5. **距离函数**：桥接工程 `distance/` 模块

## 参考资料

- SQLite 虚拟表文档：https://www.sqlite.org/vtab.html
- SQLite VSS 源码：https://github.com/asg017/sqlite-vss
- 工程向量 API：`engineering/include/db/api/vector_api.h`
