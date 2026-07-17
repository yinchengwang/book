# LSH 局部敏感哈希索引

## Purpose

实现 LSH (Locality-Sensitive Hashing) 索引，支持二值向量和浮点向量的近似最近邻搜索。

## Requirements

### Requirement: LSH 索引基础操作

LSH 索引 SHALL 支持以下核心操作：

| 操作 | 函数签名 | 说明 |
|------|----------|------|
| 创建 | `lsh_index_t *lsh_create(int n_hash, int n_tables, int dims)` | 创建 LSH 索引 |
| 训练 | `int lsh_train(lsh_index_t *idx, int n, const float *vectors)` | 训练哈希函数 |
| 添加向量 | `int lsh_add(lsh_index_t *idx, int n, const float *vectors, int *ids)` | 添加向量 |
| 添加二值向量 | `int lsh_add_binary(lsh_index_t *idx, int n, const uint8_t *vectors, int *ids)` | 添加二值向量 |
| 搜索 | `int lsh_search(lsh_index_t *idx, const float *query, int k, int *ids, float *distances)` | K 近邻搜索 |
| 批量搜索 | `int lsh_search_batch(lsh_index_t *idx, int nq, const float *queries, int k, int *ids, float *distances)` | 批量搜索 |
| 保存 | `int lsh_save(lsh_index_t *idx, const char *path)` | 保存索引 |
| 加载 | `lsh_index_t *lsh_load(const char *path)` | 加载索引 |
| 销毁 | `void lsh_destroy(lsh_index_t *idx)` | 释放资源 |

### Requirement: LSH 类型支持

LSH 索引 SHALL 支持以下 LSH 类型：

| 类型 | 说明 | 适用场景 | 距离度量 |
|------|------|----------|----------|
| `LSH_BITWISE` | 比特采样 LSH | 二值向量 | 汉明距离 |
| `LSH_PSTABLE` | p-stable LSH | 浮点向量 | L2 距离 |
| `LSH_SIMHASH` | SimHash | 浮点向量 | Cosine 相似度 |
| `LSH_MULTI` | 多_probe LSH | 通用 | 可配置 |

### Requirement: LSH 核心数据结构

```c
// LSH 类型
typedef enum lsh_type {
    LSH_BITWISE = 0,
    LSH_PSTABLE = 1,
    LSH_SIMHASH = 2,
} lsh_type_t;

// 哈希函数
typedef struct lsh_hash_func {
    lsh_type_t type;
    union {
        struct {
            int *indices;           // 采样的维度索引
            float *thresholds;      // 阈值
            int n_samples;
        } bitwise;
        struct {
            float *a;               // 随机投影向量
            float b;                // 偏移
        } pstable;
        struct {
            float *v;               // 随机超平面
        } simhash;
    } params;
} lsh_hash_func_t;

// LSH 哈希表
typedef struct lsh_table {
    uint64_t *hashes;              // 哈希值数组
    int *ids;                      // 对应的向量 ID
    int *capacity;                 // 桶容量
    int *size;                     // 当前大小
    int n_buckets;                 // 桶数量
} lsh_table_t;

// LSH 索引
typedef struct lsh_index {
    lsh_type_t type;
    int n_hash;                    // 每个表的哈希函数数
    int n_tables;                  // 哈希表数量
    int dims;                      // 向量维度
    int n_vectors;                  // 向量数量

    lsh_hash_func_t *hash_funcs;   // 哈希函数
    lsh_table_t *tables;           // 哈希表数组

    // 二值向量支持
    uint8_t *binary_vectors;       // 存储的二值向量
    int binary_vector_size;        // 每个向量的字节数
} lsh_index_t;
```

### Requirement: LSH 算法实现

#### Bitwise LSH (汉明距离)

```
对每个维度采样，得到二值签名：
h(x) = [x[i1] > t1, x[i2] > t2, ..., x[ik] > tk]
```

#### p-stable LSH (L2 距离)

```
a 是均值为 0，方差为 1 的正态分布向量
b 是 [0, r) 均匀分布
h(x) = floor((a·x + b) / r)
```

#### SimHash (Cosine 距离)

```
1. 计算向量投影 v = a·x
2. 如果 v > 0，结果位为 1，否则为 0
3. 多哈希位取平均
```

### Requirement: 搜索流程

```
1. 计算 Query 的 L 个哈希值
2. 在每个哈希表中查找相同的桶
3. 收集候选向量（OR-amplification）
4. 计算 Query 与候选向量的精确距离
5. 返回 top-k
```

#### Scenario: 二值向量搜索

- **WHEN** 用户有 100 万个 128 维二值向量
- **AND** 使用 Bitwise LSH (n_hash=10, n_tables=5)
- **THEN** 每个向量的签名长度为 50 位
- **AND** 搜索只需比较 5 个桶中的向量

#### Scenario: 浮点向量 L2 搜索

- **WHEN** 用户使用 p-stable LSH
- **AND** n_hash=20, n_tables=8
- **THEN** LSH 签名长度为 160 位
- **AND** 使用多表 OR 组合提高召回率
