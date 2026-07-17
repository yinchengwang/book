# DiskANN 索引优化提案

## Why

当前 DiskANN 实现为单图索引，在超大规模数据（亿级向量）场景下面临以下挑战：

1. **构建瓶颈**：单次 Vamana 构图 O(N·L·R) 复杂度，亿级数据无法一次性构建
2. **搜索效率**：全图搜索对于超大规模数据检索效率降低
3. **动态更新**：不支持高效的增量更新，频繁插入需要重建

本变更为 DiskANN 添加三种优化方案：Merge-Vamana（分图合并）、SPANN（分区索引）、FreshDiskANN（增量更新），支持通过参数灵活选择启用哪种优化。

## What Changes

### 1. Merge-Vamana（子图合并）

- 支持将多个子图合并为全局图
- 分区策略：支持 Random、K-Means、坐标范围三种分区方式
- 参数化：分区数量、重叠比例、合并策略

### 2. SPANN（分区层次索引）

- 将向量空间划分为多个分区
- 搜索时只访问相关分区，减少 IO
- 轻量级分区路由元数据

### 3. FreshDiskANN（增量更新）

- 支持动态区（Fresh）和静态区（Frozen）分离
- 动态区使用内存索引，支持高频插入
- 定期合并动态区到静态区
- 参数化：动态区容量、合并阈值

### 4. 统一参数配置

所有优化通过独立的配置结构和统一的 `diskann_config_t` 结构控制：

```c
/* 量化配置 */
typedef struct diskann_quantization_config {
    bool enabled;
    quantization_type_t type;  // PQ/SQ/LVQ/RQ
    int32_t subquantizers;     // m: 子空间数
    int32_t bits;              // bits: 4/6/8
} diskann_quantization_config_t;

/* Merge-Vamana 配置 */
typedef struct diskann_merge_config {
    bool enabled;
    int32_t partition_method;  // 0=auto, 1=random, 2=kmeans
    int32_t partition_count;   // 分区数
    float overlap_ratio;       // 重叠比例
} diskann_merge_config_t;

/* SPANN 配置 */
typedef struct diskann_spann_config {
    bool enabled;
    int32_t max_partitions;    // 最大分区数
    int32_t min_partition_size; // 每分区最小向量数
    int32_t search_partitions; // 搜索时最多访问分区数
} diskann_spann_config_t;

/* FreshDiskANN 配置 */
typedef struct diskann_fresh_config {
    bool enabled;
    int32_t fresh_capacity;     // 动态区容量
    int32_t merge_threshold;   // 触发合并的阈值
} diskann_fresh_config_t;

/* 存储配置 */
typedef struct diskann_storage_config {
    int32_t page_size;
    int32_t frozen_point_count;
} diskann_storage_config_t;

/* 统一配置入口 */
typedef struct diskann_config {
    /* 基础参数 */
    int32_t dims;
    int32_t index_size;                    // R: 目标邻居数
    int32_t build_search_list_size;         // L: 构图候选数
    distance_metric_t metric;

    /* 各模块配置 */
    diskann_quantization_config_t quantization;
    diskann_merge_config_t merge;
    diskann_spann_config_t spann;
    diskann_fresh_config_t fresh;
    diskann_storage_config_t storage;
} diskann_config_t;
```

## Capabilities

### New Capabilities

- `diskann-merge-vamana`: 子图合并优化，支持多分区独立构建后合并
- `diskann-spann`: 分区层次索引，支持选择性分区访问
- `diskann-fresh`: 增量更新模式，支持动态区/静态区分离
- `diskann-unified-config`: 统一的参数配置结构，支持按需启用各优化

### Modified Capabilities

- 无（现有 DiskANN 功能保持不变）

## Impact

### 受影响代码

| 文件 | 变更类型 | 说明 |
|------|---------|------|
| `include/index/vector_index/diskann/diskann.h` | 修改 | 扩展配置结构，添加优化参数 |
| `src/index/vector_index/diskann/diskann_private.h` | 修改 | 添加子图合并、分区、动态区相关结构 |
| `src/index/vector_index/diskann/diskann_merge.c` | 新增 | Merge-Vamana 合并逻辑 |
| `src/index/vector_index/diskann/diskann_spann.c` | 新增 | SPANN 分区索引逻辑 |
| `src/index/vector_index/diskann/diskann_fresh.c` | 新增 | FreshDiskANN 增量更新逻辑 |
| `src/index/vector_index/diskann/diskann_partition.c` | 新增 | 分区策略实现 |

### 新增依赖

- 无外部依赖，复用现有 K-Means 算法

### 与量化变更的关联

本变更依赖 `quantization-enhancement` 变更中定义的新量化器类型（SQ/RabitQ），需要在该变更完成后集成。
