# Bloom Filter 规格定义

## Purpose

Bloom Filter 是一种空间效率极高的概率数据结构，用于快速判断一个元素是否可能存在于集合中。它支持 add（添加）和 query（查询）操作，可能产生假阳性（不存在但报告存在），但绝不会产生假阴性（存在但报告不存在）。

## Requirements

### Requirement: Bloom Filter 基本操作

Bloom Filter SHALL 支持以下基本操作：
- `bloom_create(config)` — 根据配置创建新的 Bloom Filter
- `bloom_add(filter, key, keylen)` — 添加一个元素
- `bloom_query(filter, key, keylen)` — 查询元素是否存在，返回 true（可能存在）或 false（一定不存在）
- `bloom_destroy(filter)` — 释放内存

#### Scenario: 创建指定容量的 Bloom Filter

- **WHEN** 用户调用 `bloom_create` 并传入 `expected_items=1000, false_positive_rate=0.01`
- **THEN** 系统 SHALL 创建一个能容纳约 1000 个元素、假阳性率约为 1% 的 Bloom Filter

#### Scenario: 添加元素

- **WHEN** 用户向 Bloom Filter 添加元素 "hello"
- **THEN** 后续对该元素的查询 SHALL 返回 true

#### Scenario: 查询不存在的元素（无假阳性场景）

- **WHEN** 用户向 Bloom Filter 添加元素 "a", "b", "c"
- **WHEN** 用户查询元素 "d"（未添加）
- **THEN** 如果没有哈希冲突，查询 SHALL 返回 false

#### Scenario: 假阳性场景

- **WHEN** Bloom Filter 接近容量上限
- **WHEN** 用户查询一个未添加的元素
- **THEN** 查询 MAY 返回 true（假阳性）
- **THEN** 系统 SHALL 确保假阳性率不超过配置的阈值

### Requirement: 参数自动计算

系统 SHALL 根据用户提供的参数自动计算最优配置：
- 位数组大小：`m = -n * ln(p) / (ln(2)^2)`
- 哈希函数数量：`k = (m/n) * ln(2)`
- 其中 n = 预期元素数，p = 假阳性率

#### Scenario: 参数边界值

- **WHEN** 用户传入 `false_positive_rate=0` 或 `false_positive_rate>=1`
- **THEN** 系统 SHALL 使用默认值 0.01

### Requirement: 多哈希函数实现

Bloom Filter SHALL 使用多个独立的哈希函数来计算位数组位置。

#### Scenario: 哈希函数生成

- **WHEN** 创建 Bloom Filter 时
- **THEN** 系统 SHALL 生成 k 个哈希函数
- **THEN** 每个哈希函数 SHALL 对应位数组中的一个位置（`position = hash_i(key) % m`）

## Technical Details

### 数据结构

```c
typedef struct bloom_filter {
    uint8_t* bits;       // 位数组
    size_t   size;       // 位数组大小（位数）
    size_t   item_count; // 已添加元素数量
    size_t   hash_count; // 哈希函数数量
} bloom_filter_t;

typedef struct bloom_config {
    size_t   expected_items;      // 预期元素数量
    double   false_positive_rate; // 期望假阳性率 (0-1)
} bloom_config_t;
```

### 哈希函数实现

使用双哈希法（double hashing）生成 k 个哈希值：
```
h(i, key) = h1(key) + i * h2(key) mod m
其中：
  h1(key) = FNV-1a hash
  h2(key) = FNV-1a hash of (key + salt)
```

### 空间计算

对于 n 个元素和 p 的假阳性率：
- 最优位数组大小：`m = -n * ln(p) / (ln(2)^2)`
- 最优哈希函数数量：`k = (m/n) * ln(2)`

示例：
| n (元素数) | p (假阳性率) | m (位数) | k (哈希函数数) |
|------------|--------------|----------|----------------|
| 1000       | 0.01         | ~9586    | 10             |
| 10000      | 0.01         | ~95858   | 10             |
| 100000     | 0.001        | ~1.44M   | 10             |