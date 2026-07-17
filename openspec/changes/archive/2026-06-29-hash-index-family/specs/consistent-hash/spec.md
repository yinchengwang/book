# Consistent Hash 规格定义

## Purpose

Consistent Hash 是一种分布式系统中的数据分布算法，用于在节点增减时最小化数据迁移。它将节点和数据映射到同一个哈希环上，数据被分配到哈希环上顺时针遇到的第一个节点。一致性哈希是 Redis Cluster、Cassandra、DynamoDB 等分布式系统的核心组件。

## Requirements

### Requirement: 一致性哈希基本操作

一致性哈希环 SHALL 支持以下基本操作：
- `consistent_hash_create()` — 创建新的哈希环
- `consistent_hash_add_node(ring, node_id, vnode_count)` — 添加物理节点（带虚拟节点）
- `consistent_hash_remove_node(ring, node_id)` — 移除物理节点
- `consistent_hash_find_node(ring, key, keylen)` — 查找 key 所属的节点
- `consistent_hash_destroy(ring)` — 释放内存

#### Scenario: 创建空哈希环

- **WHEN** 用户调用 `consistent_hash_create()`
- **THEN** 系统 SHALL 创建一个空的哈希环
- **THEN** 环上没有节点

#### Scenario: 添加节点

- **WHEN** 用户向哈希环添加节点 "node-A" 并指定 150 个虚拟节点
- **THEN** 150 个虚拟节点 SHALL 均匀分布在哈希环上
- **WHEN** 用户查询 key "hello" 的归属节点
- **THEN** 返回 SHALL 是 "node-A"（如果 hello 的哈希值在 node-A 的 vnode 范围内）

#### Scenario: 多节点场景

- **WHEN** 用户向哈希环添加 3 个节点：A、B、C
- **WHEN** 用户查询多个 key 的归属
- **THEN** 归属结果 SHALL 分布在 A、B、C 上
- **THEN** 每个节点的负载 SHALL 大致均衡（虚拟节点足够多时）

#### Scenario: 移除节点

- **WHEN** 用户从哈希环移除节点 B
- **THEN** 原来分配给 B 的数据 SHALL 迁移到顺时针下一个节点
- **THEN** 其他节点的数据 SHALL 不受影响

### Requirement: 虚拟节点（vnode）机制

系统 SHALL 使用虚拟节点来平衡负载分布。

#### Scenario: 虚拟节点命名

- **WHEN** 添加物理节点 "node-A" 时
- **THEN** 虚拟节点命名格式为 "node-A#0", "node-A#1", ..., "node-A#N"
- **THEN** 每个虚拟节点 SHALL 有独立的哈希位置

#### Scenario: 虚拟节点数量

- **WHEN** 用户添加节点时未指定 vnode 数量
- **THEN** 系统 SHALL 使用默认值 150

#### Scenario: 负载均衡验证

- **WHEN** 哈希环上有 3 个节点，每个 150 个虚拟节点
- **WHEN** 插入 10000 个随机 key
- **THEN** 每个节点分配的 key 数量 SHALL 相差不超过 20%

### Requirement: 节点查询

系统 SHALL 提供高效的节点查询接口。

#### Scenario: 顺时针查找

- **WHEN** 用户调用 `consistent_hash_find_node` 查询 key "test"
- **THEN** 系统 SHALL 计算 `hash("test")`
- **THEN** 系统 SHALL 在排序的虚拟节点数组中找到第一个 hash >= key_hash 的 vnode
- **THEN** 如果没有找到，则返回第一个 vnode（绕回）

#### Scenario: 节点增删影响范围

- **WHEN** 添加一个新节点
- **THEN** 只有新节点顺时针前一个节点的部分数据会迁移
- **THEN** 迁移的数据量约为 1/(N+1) 的总数据

- **WHEN** 移除一个现有节点
- **THEN** 该节点的 数据会全部迁移到顺时针下一个节点
- **THEN** 其他节点的数据不受影响

## Technical Details

### 数据结构

```c
typedef struct ch_vnode {
    uint32_t hash;     // 虚拟节点在环上的位置
    char*    node_id;  // 物理节点 ID
    size_t   vnode_idx; // 虚拟节点序号
} ch_vnode_t;

typedef struct consistent_hash {
    ch_vnode_t* vnodes;      // 虚拟节点数组（按 hash 排序）
    size_t      vnode_count; // 虚拟节点总数
    size_t      node_count;  // 物理节点数
    char**      node_ids;    // 物理节点 ID 列表
} consistent_hash_t;
```

### 核心算法

```
查找算法：
1. 计算 key_hash = hash(key)
2. 在 vnodes 数组中二分查找 lower_bound(key_hash)
3. 如果 found < vnode_count，返回 vnodes[found]
4. 否则返回 vnodes[0]（绕回）

添加节点：
1. 为每个虚拟节点计算 hash("node_id#i")
2. 将所有 vnode 加入数组
3. 按 hash 排序

移除节点：
1. 从数组中移除该物理节点的所有虚拟节点
2. 重新排序
```

### 与简单哈希取模对比

| 场景 | 简单取模 hash(key) % N | 一致性哈希 |
|------|------------------------|------------|
| 添加节点 | 几乎 100% 数据需要迁移 | 只有 1/(N+1) 数据迁移 |
| 移除节点 | 几乎 100% 数据需要迁移 | 只有 1/N 数据迁移 |
| 负载均衡 | 取决于哈希函数 | 依赖虚拟节点数量 |
| 实现复杂度 | O(1) | O(log V + log N) |

### API 设计

```c
// 创建/销毁
consistent_hash_t* ch_create(void);
void               ch_destroy(consistent_hash_t* ring);

// 节点管理
int ch_add_node(consistent_hash_t* ring, const char* node_id, size_t vnode_count);
int ch_remove_node(consistent_hash_t* ring, const char* node_id);

// 查询
const char* ch_find_node(const consistent_hash_t* ring, const void* key, size_t keylen);

// 统计
size_t ch_get_node_count(const consistent_hash_t* ring);
size_t ch_get_vnode_count(const consistent_hash_t* ring);
```