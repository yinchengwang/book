# GiST 广义搜索树索引

## Purpose

实现 GiST (Generalized Search Tree) 索引，支持任意自定义数据类型和操作符的空间索引扩展。

## Requirements

### Requirement: GiST 索引基础操作

GiST 索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `gist_index_t *gist_create(void)` | 创建 GiST 索引 |
| 插入 | `int gist_insert(gist_index_t *idx, const void *key, const void *value)` | 插入键值对 |
| 范围查询 | `int gist_range_query(gist_index_t *idx, const void *query_box, void **results, int *count)` | 范围查询 |
| KNN 查询 | `int gist_knn_query(gist_index_t *idx, const void *point, int k, void **results, float *distances)` | K 近邻查询 |
| 删除 | `int gist_delete(gist_index_t *idx, const void *key)` | 删除键 |
| 销毁 | `void gist_destroy(gist_index_t *idx)` | 释放资源 |

### Requirement: GiST 核心数据结构

```c
// GiST 操作符接口（用户实现）
typedef struct gist_ops {
    // 合并子节点的 bounding box
    void *(*union_fn)(void **children, int count);
    // 判断两个 bbox 是否一致
    int (*consistent_fn)(const void *bbox, const void *query);
    // 计算两个 bbox 的距离
    float (*distance_fn)(const void *bbox, const void *point);
    // 计算 bbox 大小
    int (*size_fn)(const void *bbox);
    // 释放 bbox
    void (*free_fn)(void *bbox);
} gist_ops_t;

// GiST 节点
typedef struct gist_node {
    void *bbox;                    // 边界框
    int is_leaf;                   // 是否叶子节点
    int n_entries;                 // 条目数
    struct gist_entry {
        void *key;
        void *value;
        struct gist_node *child;   // 非叶子节点指向子节点
    } *entries;
    struct gist_node *parent;
} gist_node_t;

// GiST 索引
typedef struct gist_index {
    gist_node_t *root;
    gist_ops_t ops;
    int size;
} gist_index_t;
```

### Requirement: 空间操作符支持

GiST SHOULD 支持以下空间操作符：

| 操作符 | 说明 | 用途 |
|--------|------|------|
| `<<` | 严格左 | A 在 B 左边 |
| `&<` | 左重叠 | A 左边界在 B 内 |
| `<<|` | 严格下 | A 在 B 下边 |
| `&<|` | 下重叠 | A 下边界在 B 内 |
| `>>` | 严格右 | A 在 B 右边 |
| `&>` | 右重叠 | A 右边界在 B 内 |
| `|>>` | 严格上 | A 在 B 上边 |
| `|&>` | 上重叠 | A 上边界在 B 内 |
| `&&` | 覆盖 | A 与 B 相交 |
| `@>` | 包含 | A 包含 B |
| `<@` | 被包含 | A 被 B 包含 |
| `~=` | 相同 | A 与 B 相同 |
| `##` | 覆盖或接触 | A 覆盖或接触 B |

### Requirement: 自定义操作符注册

用户 SHALL 能够注册自定义操作符：

```c
int gist_register_op(gist_index_t *idx,
                     const char *op_name,
                     gist_ops_t *ops);
```

#### Scenario: 创建 R-Tree 风格的 GiST

- **WHEN** 用户调用 `gist_create_with_bbox(sizeof(rect_t), &rect_ops)`
- **THEN** 系统 SHALL 使用矩形操作符构建 GiST
- **AND** 支持 Within/Contains/Intersects 查询

#### Scenario: 注册自定义类型

- **WHEN** 用户实现 `gist_ops_t` 并调用 `gist_register_op`
- **THEN** 系统 SHALL 支持该自定义类型的 GiST 索引
