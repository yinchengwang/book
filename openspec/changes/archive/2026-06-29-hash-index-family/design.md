# Hash 索引族实现设计

## Context

项目目前已实现两种经典哈希索引：
- `src/index/hash/hash/` — PG 风格 Linear Hash
- `src/index/hash/cceh/` — CCEH（Cache-Line Efficient Hashing）

这两种都是基于桶的哈希表，用于键值存储。本变更将引入：
1. **Filter 类索引**：Bloom、Cuckoo、Xor，用于快速存在性判断（无删除需求或支持删除）
2. **Consistent Hash**：将 `src/algo/ds/distributed_index.c` 的演示代码升级为完整实现

约束：
- 纯内存实现，不涉及持久化
- 遵循项目 C11/C++17 标准
- 代码风格与现有代码保持一致（中文注释）

## Goals / Non-Goals

**Goals:**
- 实现标准 Bloom Filter，支持 add/query
- 实现 Cuckoo Filter，支持 add/query/delete
- 实现 Xor Filter，空间优化版本
- 升级 Consistent Hash，支持 vnode 和节点管理

**Non-Goals:**
- 持久化支持（WAL、checkpoint）
- 并发控制（Filter 类天然只读或简单并发）
- 分布式网络通信（Consistent Hash 只实现本地节点管理）

## Decisions

### Decision 1: Filter 类索引放在 `src/index/hash/` 下

**选择**：Bloom/Cuckoo/Xor 放在 `src/index/hash/` 子目录

**理由**：
- 与现有 pg_hash、cceh 同属哈希索引族
- Filter 是概率数据结构，与精确哈希表有区别但概念相关
- 方便与向量索引配合（Bloom Filter 常用于 HNSW/DiskANN 的快速剪枝）

### Decision 2: Consistent Hash 升级现有 demo

**选择**：直接扩展 `src/algo/ds/distributed_index.c`

**理由**：
- 已有演示代码，只需扩展 API
- `algo/ds/` 本就是数据结构和算法演示的位置
- Consistent Hash 不是"索引"，是分布式系统基础组件

### Decision 3: Filter 参数配置方式

**选择**：在创建时指定容量和假阳性率，由库自动计算最优参数

```c
// API 设计示例
typedef struct {
    size_t expected_items;  // 预期元素数量
    double false_positive_rate;  // 期望假阳性率 (默认 0.01)
} bloom_config_t;

bloom_filter_t* bloom_create(const bloom_config_t* config);
```

**理由**：
- 用户只需关心容量和假阳性率
- 内部自动计算最优的位数组大小和哈希函数数量
- 参考 Google Guava、Python `bloom-filter` 库的常见 API

### Decision 4: Cuckoo Filter 踢出策略

**选择**：简单版 cuckoo hashing，只支持 2 路（每个桶 2 个槽位）

```c
// 桶结构
typedef struct {
    fingerprint_t fp[2];  // 两个指纹槽
    bool occupied[2];     // 是否占用
} cuckoo_bucket_t;
```

**理由**：
- 完整版 d-ary Cuckoo 复杂度高
- 2 路在大多数场景下已足够高效
- 简化实现便于理解和面试回答

### Decision 5: Xor Filter 使用 Fuse Filter 变体

**选择**：实现 Xor Filter 的简化版本（Xor16）

**理由**：
- Xor Filter 有多种变体（Xor16、Xor32、Xor+）
- 简化版保留核心思想（XOR 操作构造可逆映射）
- 完整版论文算法较复杂，作为后续优化方向

## Risks / Trade-offs

| 风险 | 影响 |  Mitigation |
|------|------|-------------|
| Cuckoo Filter 删除实现复杂 | 可能出现循环踢出 | 设置最大踢出次数，超过则返回错误 |
| Xor Filter 构造失败 | 某些输入可能构造失败 | 使用确定性种子，支持重试 |
| Consistent Hash 负载不均 | vnode 数量少时可能倾斜 | 默认每个物理节点 150 个 vnode |

## Migration Plan

1. **新增目录和文件**
   - 创建 `src/index/hash/bloom/`
   - 创建 `src/index/hash/cuckoo/`
   - 创建 `src/index/hash/xor/`
   - 扩展 `src/algo/ds/distributed_index.c`

2. **更新 CMakeLists.txt**
   - 在 `src/index/hash/CMakeLists.txt` 添加子目录
   - 在 `test/vector_index/hash/CMakeLists.txt` 添加测试

3. **测试验证**
   - 每个 Filter 单独测试基本功能
   - Consistent Hash 测试节点增删后的数据迁移

## Open Questions

1. **Cuckoo Filter 指纹大小**：使用 1 字节（支持 ~256 槽/桶）还是 2 字节（更少冲突）？
2. **Xor Filter 最大容量**：是否需要限制最大元素数量以便预分配内存？
3. **测试覆盖率**：是否需要 100% 分支覆盖，还是只测核心路径？