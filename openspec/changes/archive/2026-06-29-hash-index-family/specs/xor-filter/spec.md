# Xor Filter 规格定义

## Purpose

Xor Filter 是一种比 Bloom Filter 空间效率更高的概率数据结构，基于 XOR 操作构造。它使用约 1.23n 位空间（Bloom 需要约 1.44n 位），适用于对空间敏感且不需要删除操作的场景。

## Requirements

### Requirement: Xor Filter 基本操作

Xor Filter SHALL 支持以下基本操作：
- `xor_filter_create(config)` — 根据配置创建新的 Xor Filter
- `xor_filter_add(filter, key, keylen)` — 添加一个元素
- `xor_filter_query(filter, key, keylen)` — 查询元素是否存在
- `xor_filter_destroy(filter)` — 释放内存

#### Scenario: 创建 Xor Filter

- **WHEN** 用户调用 `xor_filter_create` 并传入 `expected_items=1000`
- **THEN** 系统 SHALL 创建一个能容纳约 1000 个元素的 Xor Filter
- **THEN** 内部 SHALL 分配 3 个大小为 1.23n 的数组

#### Scenario: 添加元素

- **WHEN** 用户向 Xor Filter 添加元素 "hello"
- **THEN** 后续对该元素的查询 SHALL 返回 true

#### Scenario: 查询元素

- **WHEN** 用户向 Xor Filter 添加元素 "a", "b", "c"
- **WHEN** 用户查询元素 "b"
- **THEN** 查询 SHALL 返回 true

#### Scenario: 查询不存在的元素

- **WHEN** 用户向 Xor Filter 添加元素 "a", "b", "c"
- **WHEN** 用户查询元素 "d"（未添加）
- **THEN** 查询 SHALL 返回 false（Xor Filter 无假阳性）

### Requirement: XOR 操作构造

Xor Filter SHALL 使用 XOR 操作来编码集合成员关系。

#### Scenario: 数据结构

- **WHEN** 创建 Xor Filter 时
- **THEN** 系统 SHALL 分配 3 个大小为 m 的数组（m ≈ 1.23n）
- **THEN** 每个数组元素 SHALL 是 3 字节（24 位）整数

#### Scenario: 位置计算

- **WHEN** 对元素 x 计算其 3 个位置时
- **THEN** 系统 SHALL 使用：`h1 = hash(x, 0) % m`, `h2 = hash(x, 1) % m`, `h3 = hash(x, 2) % m`

#### Scenario: 添加时的 XOR 编码

- **WHEN** 添加元素 x 时
- **THEN** 系统 SHALL 计算指纹 `fp = hash(x, 3)`
- **THEN** 系统 SHALL 执行：`arr0[h1] ^= fp`, `arr1[h2] ^= fp`, `arr2[h3] ^= fp`

#### Scenario: 查询时的解码

- **WHEN** 查询元素 x 时
- **THEN** 系统 SHALL 计算指纹 `fp = hash(x, 3)`
- **THEN** 系统 SHALL 计算 `xored = arr0[h1] ^ arr1[h2] ^ arr2[h3]`
- **THEN** 如果 `xored == fp`，元素可能存在；否则一定不存在

### Requirement: 构造算法

Xor Filter SHALL 使用标准的三数组构造算法。

#### Scenario: 构造过程

- **WHEN** 构造 Xor Filter 时（批量添加完成后）
- **THEN** 系统 SHALL 首先收集所有元素的指纹
- **THEN** 系统 SHALL 使用贪婪算法将元素分配到 3 个位置
- **THEN** 如果构造失败（最大迭代次数），返回错误

#### Scenario: 构造失败处理

- **WHEN** 构造算法在 MAX_ITERATIONS 次迭代后仍未收敛
- **THEN** 系统 SHALL 返回错误码
- **THEN** 用户 SHALL 重新尝试（使用不同的哈希种子）

## Technical Details

### 数据结构

```c
#define XOR_FILTER_FP_SIZE 3  // 指纹大小（字节）

typedef struct xor_filter {
    uint32_t* arr0;    // 第一个数组
    uint32_t* arr1;    // 第二个数组
    uint32_t* arr2;    // 第三个数组
    size_t    size;    // 每个数组的大小
    size_t    item_count; // 元素数量
} xor_filter_t;

typedef struct xor_filter_config {
    size_t expected_items;  // 预期元素数量
} xor_filter_config_t;
```

### 空间分析

| 元素数 n | Bloom Filter (1% FP) | Xor Filter |
|----------|---------------------|------------|
| 1000     | ~9.6 Kbits          | ~12.3 Kbits (含结构) |
| 10000    | ~96 Kbits           | ~123 Kbits |
| 100000   | ~1.44 Mbits         | ~1.85 Mbits |

注意：Xor Filter 的理论空间是 1.23n 位，但实现开销可能导致总空间略大。

### 与 Bloom Filter 对比

| 特性 | Bloom Filter | Xor Filter |
|------|--------------|------------|
| 空间 | ~1.44n 位    | ~1.23n 位  |
| 删除支持 | ❌ | ❌ |
| 假阳性 | 有（~1%） | 有（~1%） |
| 构造复杂度 | O(n) | O(n log n) |
| 查询复杂度 | O(k) | O(3) |