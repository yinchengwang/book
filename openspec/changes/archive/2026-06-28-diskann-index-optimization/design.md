# DiskANN 索引优化设计文档

## Context

### 背景

当前 DiskANN 实现为单图索引，存在于 `src/index/vector_index/diskann/` 目录。现有实现支持：

- Vamana 图构建
- PQ 量化压缩
- 块式持久化存储
- 增量插入与删除

### 现有架构

```
┌─────────────────────────────────────────────────────────────────────┐
│                         DiskANN 架构                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  diskann_t                                                          │
│  ├── vectors[]        原始向量                                        │
│  ├── neighbors[]      邻接表                                          │
│  ├── quantizer        量化器                                          │
│  ├── codes[]          PQ 编码                                        │
│  └── ...                                                            │
│                                                                      │
│  搜索流程: 冻结点 → Best-First 遍历 → 精确重排                        │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 约束

- 纯 C 实现，无外部依赖（除 algo 模块）
- 保持现有 API 兼容，新增配置参数可选
- 各优化方案可独立启用或组合使用

## Goals / Non-Goals

**Goals:**
- 实现 Merge-Vamana：支持分图构建后合并
- 实现 SPANN：支持分区索引减少搜索范围
- 实现 FreshDiskANN：支持动态增量更新
- 统一配置接口，支持参数化启用各优化

**Non-Goals:**
- 不改变现有基础 API 接口
- 不实现磁盘直读模式（未来可扩展）
- 不实现真正的并发写入（未来可扩展）

## 三大优化方案详解

### 1. Merge-Vamana 设计

#### 核心思想

```
┌─────────┐  ┌─────────┐  ┌─────────┐
│ 子图 G1  │  │ 子图 G2  │  │ 子图 G3  │     ← 3 个分区独立构建
│  100万  │  │  100万  │  │  100万  │
└────┬────┘  └────┬────┘  └────┬────┘
     │            │            │
     └────────────┼────────────┘
                  ↓
         ┌─────────────────┐
         │  Merge 合并     │
         │  1. 节点去重   │
         │  2. 边合并     │
         │  3. 跨分区边   │
         └────────┬────────┘
                  ↓
         ┌─────────────────┐
         │  全局图 G      │
         │   300万节点    │
         └─────────────────┘
```

#### 分区策略

| 策略 | 方法 | 适用场景 |
|------|------|---------|
| Random | 随机分配向量到分区 | 数据分布均匀 |
| K-Means | K-Means 聚类分区 | 数据有簇结构 |
| Coordinate-Range | 按坐标范围切分 | 地理/坐标数据 |

#### Merge 算法

```c
/**
 * Merge-Vamana 合并流程
 *
 * Step 1: 收集所有子图的节点和边
 * Step 2: 节点去重（跨分区相同向量）
 * Step 3: 边合并（保留所有子图边）
 * Step 4: 添加跨分区边（使用全局入口点）
 * Step 5: 局部 Robust Prune（每节点独立剪枝）
 */
int diskann_merge_graphs(diskann_t *merged,
                         diskann_t **subgraphs,
                         int32_t count);
```

#### 配置结构

```c
typedef struct diskann_merge_config {
    bool enabled;                    // 是否启用合并模式
    int32_t partition_method;        // 分区策略
    int32_t partition_count;         // 分区数量
    float overlap_ratio;             // 重叠比例（0.0-1.0）
} diskann_merge_config_t;
```

### 2. SPANN 设计

#### 核心思想

SPANN (SParse ANN) 通过将向量空间分区，使搜索只访问相关分区：

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SPANN 搜索流程                                 │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│   Query ──→ Partition Selection (路由)                              │
│                   │                                                 │
│                   ├──→ P1 (不相关, skip)  ✓                        │
│                   ├──→ P2 (相关, 搜索子图)  ✓                       │
│                   ├──→ P3 (相关, 搜索子图)  ✓                       │
│                   └──→ P4 (不相关, skip)  ✓                         │
│                                                                      │
│   关键: 需要轻量级分区元数据快速判断相关性                            │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

#### 分区元数据

```c
typedef struct diskann_partition {
    int32_t id;                      // 分区 ID
    float center[DIMS];              // 分区中心
    float radius;                    // 分区半径
    int32_t start;                   // 向量起始索引
    int32_t count;                   // 向量数量
} diskann_partition_t;
```

#### 搜索路由算法

```c
/**
 * SPANN 分区选择算法
 *
 * 给定查询向量 q 和最大访问分区数 search_partitions:
 *
 * Step 1: 计算 q 到所有分区的距离
 * Step 2: 按距离排序
 * Step 3: 选择最近的 search_partitions 个分区
 * Step 4: 在选中的分区中执行搜索
 */
int diskann_spann_search(diskann_t *index,
                         const float *query,
                         int32_t k,
                         diskann_partition_t **selected,
                         int32_t *selected_count);
```

#### 配置结构

```c
typedef struct diskann_spann_config {
    bool enabled;                    // 是否启用 SPANN
    int32_t max_partitions;         // 最大分区数
    int32_t min_partition_size;     // 每分区最小向量数
    int32_t search_partitions;      // 搜索时最多访问分区数
} diskann_spann_config_t;
```

### 3. FreshDiskANN 设计

#### 核心思想

FreshDiskANN 将索引分为静态区（Frozen）和动态区（Fresh）：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    FreshDiskANN 内存布局                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ┌─────────────────────┐  ┌─────────────────────┐                  │
│  │   静态区 (Frozen)   │  │   动态区 (Fresh)    │                  │
│  │                     │  │                     │                  │
│  │  ┌───────────────┐  │  │  ┌───────────────┐  │                  │
│  │  │  原始向量      │  │  │  │  原始向量      │  │                  │
│  │  └───────────────┘  │  │  └───────────────┘  │                  │
│  │  ┌───────────────┐  │  │  ┌───────────────┐  │                  │
│  │  │  图结构        │  │  │  │  图结构        │  │                  │
│  │  └───────────────┘  │  │  └───────────────┘  │                  │
│  │                     │  │  ┌───────────────┐  │                  │
│  │  磁盘持久化         │  │  │  量化器        │  │                  │
│  │                     │  │  └───────────────┘  │                  │
│  └─────────────────────┘  └─────────────────────┘                  │
│                                                                      │
│  搜索: 静态区(磁盘IO) + 动态区(内存) → 合并结果                       │
│  合并: 当动态区达到 merge_threshold → 合并到静态区                     │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

#### 动态区管理

```c
typedef struct diskann_fresh_index {
    float *vectors;                 // 动态区向量
    int32_t *neighbors;            // 动态区邻接表
    int32_t capacity;              // 容量
    int32_t count;                 // 当前数量
    diskann_quantization_params_t quant_params; // 量化参数
} diskann_fresh_index_t;
```

#### 合并策略

```c
/**
 * FreshDiskANN 合并流程
 *
 * 当动态区达到 merge_threshold 时触发:
 *
 * Step 1: 将动态区向量写入磁盘
 * Step 2: 更新静态区图结构（增量构建）
 * Step 3: 清空动态区
 * Step 4: 更新搜索入口点
 */
int diskann_fresh_merge(diskann_t *index);
```

#### 配置结构

```c
typedef struct diskann_fresh_config {
    bool enabled;                    // 是否启用 Fresh 模式
    int32_t fresh_capacity;          // 动态区容量
    int32_t merge_threshold;         // 触发合并的阈值
} diskann_fresh_config_t;
```

## 统一配置架构

### 配置入口

```c
typedef struct diskann_config {
    int32_t dims;
    int32_t index_size;
    int32_t build_search_list_size;
    distance_metric_t metric;

    diskann_quantization_config_t quantization;
    diskann_merge_config_t merge;
    diskann_spann_config_t spann;
    diskann_fresh_config_t fresh;
    diskann_storage_config_t storage;
} diskann_config_t;
```

### 初始化优先级

```
┌─────────────────────────────────────────────────────────────────────┐
│                      配置初始化顺序                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  1. 基础参数 (dims, metric, index_size)                            │
│      ↓                                                               │
│  2. 量化配置 (quantization)  ─→ 创建量化器                          │
│      ↓                                                               │
│  3. 存储配置 (storage)     ─→ 初始化持久化                           │
│      ↓                                                               │
│  4. Merge 配置 (merge)     ─→ 可选，延后处理                       │
│      ↓                                                               │
│  5. SPANN 配置 (spann)     ─→ 可选，延后处理                       │
│      ↓                                                               │
│  6. Fresh 配置 (fresh)     ─→ 可选，延后处理                       │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

## 文件组织

```
src/index/vector_index/diskann/
├── diskann.h              ← 公共接口
├── diskann_private.h      ← 内部结构
├── diskann_create.c       ← 创建/销毁
├── diskann_search.c       ← 搜索逻辑
├── diskann_graph.c        ← 图操作
├── diskann_build.c        ← Vamana 构建
├── diskann_insert.c       ← 增量插入
├── diskann_delete.c       ← 删除
├── diskann_persist.c      ← 持久化
├── diskann_utils.c        ← 工具函数
├── diskann_quantization.c ← 量化集成
├── diskann_merge.c        ← 🆕 Merge-Vamana
├── diskann_spann.c        ← 🆕 SPANN
├── diskann_fresh.c        ← 🆕 FreshDiskANN
└── diskann_partition.c    ← 🆕 分区策略
```

## Risks / Trade-offs

| Risk | Description | Mitigation |
|------|-------------|------------|
| Merge 图质量 | 合并后图可能不如单次构建质量高 | 使用足够大的 overlap_ratio |
| SPANN 召回率 | 分区路由可能遗漏相关向量 | 设置足够的 search_partitions |
| Fresh 内存占用 | 动态区占用额外内存 | 设置合理的 fresh_capacity |
| 合并开销 | Fresh→Frozen 合并耗时 | 异步合并或低峰期合并 |

## Open Questions

1. **Merge 边数控制**: 合并后图边数可能膨胀，如何控制每节点最大边数？
2. **SPANN 路由精度**: 简单距离判断是否足够，是否需要更复杂的路由模型？
3. **Fresh 搜索策略**: 动态区和静态区如何分配搜索预算？
