# Cuckoo Filter 规格定义

## Purpose

Cuckoo Filter 是一种支持删除操作的空间高效概率数据结构。它使用 Cuckoo Hashing 的思想，每个桶可以存储多个元素（指纹），支持动态插入、查询和删除，比 Bloom Filter 更适合需要频繁删除的场景。

## Requirements

### Requirement: Cuckoo Filter 基本操作

Cuckoo Filter SHALL 支持以下基本操作：
- `cuckoo_create(config)` — 根据配置创建新的 Cuckoo Filter
- `cuckoo_add(filter, key, keylen)` — 添加一个元素
- `cuckoo_query(filter, key, keylen)` — 查询元素是否存在
- `cuckoo_delete(filter, key, keylen)` — 删除一个元素
- `cuckoo_destroy(filter)` — 释放内存

#### Scenario: 创建 Cuckoo Filter

- **WHEN** 用户调用 `cuckoo_create` 并传入 `expected_items=1000`
- **THEN** 系统 SHALL 创建一个能容纳约 1000 个元素的 Cuckoo Filter

#### Scenario: 添加元素

- **WHEN** 用户向 Cuckoo Filter 添加元素 "hello"
- **THEN** 后续对该元素的查询 SHALL 返回 true

#### Scenario: 查询元素

- **WHEN** 用户向 Cuckoo Filter 添加元素 "a", "b", "c"
- **WHEN** 用户查询元素 "b"
- **THEN** 查询 SHALL 返回 true

#### Scenario: 删除元素

- **WHEN** 用户向 Cuckoo Filter 添加元素 "hello"
- **WHEN** 用户删除元素 "hello"
- **THEN** 后续对该元素的查询 SHALL 返回 false

#### Scenario: 重复添加

- **WHEN** 用户向 Cuckoo Filter 添加相同元素两次
- **THEN** 该元素 SHALL 只存在一条记录
- **WHEN** 用户删除该元素一次
- **THEN** 后续查询 SHALL 返回 false

### Requirement: Cuckoo Hashing 核心机制

Cuckoo Filter SHALL 使用 2 路 Cuckoo Hashing，每个桶有 2 个槽位。

#### Scenario: 桶结构

- **WHEN** 创建 Cuckoo Filter 时
- **THEN** 每个桶 SHALL 包含 2 个指纹槽位
- **THEN** 系统 SHALL 计算每个元素的两个候选桶位置

#### Scenario: 哈希位置计算

- **WHEN** 计算元素 x 的位置时
- **THEN** 系统 SHALL 使用：`h1 = hash(x) % bucket_count` 和 `h2 = h1 XOR hash(fingerprint)`
- **THEN** 元素 SHALL 被插入到 h1 或 h2 位置的空槽位中

#### Scenario: 踢出重插入（Eviction）

- **WHEN** 两个候选桶都满了
- **THEN** 系统 SHALL 随机踢出一个桶中的元素
- **THEN** 被踢出的元素 SHALL 被插入到其另一个候选位置
- **THEN** 踢出过程 SHALL 递归进行，最多 MAX_KICK 次

#### Scenario: 踢出次数超限

- **WHEN** 踢出次数达到 MAX_KICK（默认 500）时
- **THEN** 添加操作 SHALL 返回错误码
- **THEN** 系统 SHALL 保持不变

### Requirement: 指纹设计

Cuckoo Filter SHALL 使用固定大小的指纹（默认 1 字节）。

#### Scenario: 指纹生成

- **WHEN** 添加元素时
- **THEN** 系统 SHALL 生成该元素的指纹：`fingerprint = hash(key) & 0xFF`

#### Scenario: 指纹匹配

- **WHEN** 查询元素时
- **THEN** 系统 SHALL 计算该元素的指纹
- **THEN** 系统 SHALL 检查两个候选桶中是否存在相同指纹

## Technical Details

### 数据结构

```c
#define CUCKOO_BUCKET_SIZE 2    // 每桶槽位数
#define CUCKOO_FP_SIZE 1        // 指纹字节数
#define CUCKOO_MAX_KICK 500     // 最大踢出次数

typedef struct {
    uint8_t fingerprint;  // 指纹
    bool    occupied;     // 是否占用
} cuckoo_slot_t;

typedef struct {
    cuckoo_slot_t slots[CUCKOO_BUCKET_SIZE];
} cuckoo_bucket_t;

typedef struct cuckoo_filter {
    cuckoo_bucket_t* buckets;    // 桶数组
    size_t           bucket_count; // 桶数量
    size_t           item_count;   // 元素数量
} cuckoo_filter_t;

typedef struct cuckoo_config {
    size_t expected_items;  // 预期元素数量
} cuckoo_config_t;
```

### 空间效率

- 每个元素占用：1 字节（指纹）+ 1 位（占用标记）≈ 1.125 字节
- 负载因子超过约 95% 时，插入可能失败
- 建议预分配容量为预期元素数的 1.2-1.5 倍

### 假阳性率

- 1 字节指纹的假阳性率 ≈ 1/256 ≈ 0.39%
- 这是空间和准确率的权衡