# GIN 通用倒排索引

## Purpose

实现 GIN (Generalized Inverted Index) 索引，支持 JSONB、数组、全文搜索等复合数据类型的快速检索。

## Requirements

### Requirement: GIN 索引基础结构

GIDX 索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `gin_index_t *gin_create(int max_entries)` | 创建 GIN 索引 |
| 插入 | `int gin_insert(gin_index_t *idx, const char *key, int doc_id)` | 插入 key → doc_id 映射 |
| 批量插入 | `int gin_insert_batch(gin_index_t *idx, const char **keys, int *doc_ids, int count)` | 批量插入 |
| 搜索 | `int gin_search(gin_index_t *idx, const char *query, int *results, int *count)` | 搜索包含 query 的文档 |
| 范围查询 | `int gin_range_query(gin_index_t *idx, const char *start, const char *end, int *results, int *count)` | 范围查询 |
| 删除 | `int gin_delete(gin_index_t *idx, const char *key, int doc_id)` | 删除映射 |
| 销毁 | `void gin_destroy(gin_index_t *idx)` | 释放资源 |

### Requirement: GIN 核心数据结构

GIDX 索引 SHALL 包含以下数据结构：

```c
// Posting List：同一 key 的文档 ID 列表
typedef struct posting_list {
    int doc_id;
    struct posting_list *next;
} posting_list_t;

// Posting Tree： postings 的平衡树结构
typedef struct posting_tree {
    void *root;                    // B-Tree 根节点
    int count;                     // 节点数
} posting_tree_t;

// GIN Entry：key → postings 映射
typedef struct gin_entry {
    char *key;
    void *postings;               // posting_list_t* 或 posting_tree_t*
    int is_posting_tree;          // 是否使用 posting tree
} gin_entry_t;

// GIN Index
typedef struct gin_index {
    void *tree;                   // B-Tree，存储 gin_entry_t
    int size;                      // 索引大小
    int max_entries;               // 最大条目数
} gin_index_t;
```

### Requirement: 内存管理

- 所有动态分配使用 `malloc/free`
- 字符串 key 使用 `strdup` 复制
- 索引销毁时递归释放所有节点

### Requirement: JSONB/数组支持

GIDX 索引 SHOULD 支持 JSONB 和数组类型的自动展平：

| 输入类型 | 展平策略 | 示例 |
|----------|----------|------|
| JSONB | 提取所有叶子节点 | `{"a": {"b": 1}}` → `["a", "a.b", "a.b.1"]` |
| 数组 | 索引每个元素 | `[1, 2, 3]` → `[1, 2, 3]` |
| 嵌套数组 | 递归展平 | `[[1,2], [3,4]]` → `[1, 2, 3, 4]` |

#### Scenario: JSONB 索引

- **WHEN** 用户调用 `gin_insert_json(gin, json_string, doc_id)`
- **THEN** 系统 SHALL 自动提取 JSON 所有 key-value 对并索引
- **AND** 嵌套路径使用 `.` 连接（如 `a.b.c`）

#### Scenario: 数组索引

- **WHEN** 用户调用 `gin_insert_array(gin, array_of_strings, count, doc_id)`
- **THEN** 系统 SHALL 为数组中每个元素创建 posting
