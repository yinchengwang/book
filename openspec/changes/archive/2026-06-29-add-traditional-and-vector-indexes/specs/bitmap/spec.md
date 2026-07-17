# Bitmap 位图索引

## Purpose

实现 Bitmap 位图索引，适合低基数列的高效索引。

## Requirements

### Requirement: Bitmap 索引基础操作

Bitmap 索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `bitmap_index_t *bitmap_create(int n_docs, int n_values)` | 创建 Bitmap 索引 |
| 设置值 | `int bitmap_set(bitmap_index_t *idx, int doc_id, int value)` | 设置文档值 |
| 批量设置 | `int bitmap_set_batch(bitmap_index_t *idx, int *doc_ids, int *values, int count)` | 批量设置 |
| 等值查询 | `int bitmap_eq(bitmap_index_t *idx, int value, int *doc_ids, int *count)` | 等值查询 |
| AND 查询 | `int bitmap_and(bitmap_index_t *idx, int value1, int value2, int *doc_ids, int *count)` | AND 组合查询 |
| OR 查询 | `int bitmap_or(bitmap_index_t *idx, int value1, int value2, int *doc_ids, int *count)` | OR 组合查询 |
| NOT 查询 | `int bitmap_not(bitmap_index_t *idx, int value, int *doc_ids, int *count)` | NOT 查询 |
| 范围查询 | `int bitmap_range(bitmap_index_t *idx, int min_val, int max_val, int *doc_ids, int *count)` | 范围查询 |
| 销毁 | `void bitmap_destroy(bitmap_index_t *idx)` | 释放资源 |

### Requirement: Bitmap 核心数据结构

```c
// 位图存储
typedef struct bitmap {
    uint32_t *data;               // 位数据
    int n_bits;                   // 位数
    int n_words;                  // word 数量
} bitmap_t;

// Bitmap 索引
typedef struct bitmap_index {
    int n_docs;                   // 文档总数
    int n_values;                 // 不同值的数量
    bitmap_t **bitmaps;            // 每个值的位图数组
    hash_table_t *value_map;       // 值 -> bitmap 索引
} bitmap_index_t;
```

### Requirement: 位运算支持

Bitmap 索引 SHALL 支持以下位运算：

| 运算 | 函数 | 说明 |
|------|------|------|
| AND | `bitmap_and(b1, b2)` | 交集 |
| OR | `bitmap_or(b1, b2)` | 并集 |
| XOR | `bitmap_xor(b1, b2)` | 异或 |
| NOT | `bitmap_not(b)` | 取反 |
| ANDNOT | `bitmap_andnot(b1, b2)` | b1 AND NOT b2 |

### Requirement: 压缩支持

Bitmap SHOULD 支持以下压缩格式：

| 格式 | 说明 | 适用场景 |
|------|------|----------|
| Plain | 无压缩 | 小数据集 |
| RLE | 游程编码 | 连续相同位 |
| BBC | BBC 压缩 | 中等基数 |
| Roaring | Roaring Bitmap | 大基数高性能 |

```c
typedef enum bitmap_compression {
    BITMAP_COMPRESS_NONE = 0,
    BITMAP_COMPRESS_RLE,
    BITMAP_COMPRESS_BBC,
    BITMAP_COMPRESS_ROARING,
} bitmap_compression_t;

int bitmap_set_compression(bitmap_index_t *idx, bitmap_compression_t compress);
```

### Requirement: 多列 Bitmap 索引

Bitmap 索引 SHALL 支持多列组合：

```c
typedef struct bitmap_index_multi {
    bitmap_index_t **column_indexes;
    int n_columns;
} bitmap_index_multi_t;

int bitmap_multi_and(bitmap_index_multi_t *idx, int *values, int n_columns,
                     int *doc_ids, int *count);
```

#### Scenario: 低基数列查询

- **WHEN** 用户在性别列（2 个值）上创建 Bitmap 索引
- **AND** 索引 100 万行数据
- **THEN** 每个值的位图大小为 100 万位（约 125KB）
- **AND** 等值查询只需读取一个位图

#### Scenario: 多列 AND 查询

- **WHEN** 用户执行 `WHERE gender='F' AND status='active'`
- **AND** 有多列 Bitmap 索引
- **THEN** 系统 SHALL 执行位图 AND 操作
- **AND** 一次位运算完成组合过滤
