# 向量索引页管理与图去重优化 - 技术设计

## 概述

本文档描述向量索引页管理和图去重优化的技术设计方案。

## 架构设计

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           向量引擎层 (vector_engine)                         │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────────────────────────────────────────────────────────────┐   │
│  │                      vector_index_t (抽象接口)                       │   │
│  │  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐            │   │
│  │  │  hnsw_index  │  │ vamana_index │  │ ivf_hnsw    │  ...       │   │
│  │  └──────────────┘  └──────────────┘  └──────────────┘            │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                    │                                        │
│  ┌─────────────────────────────────▼─────────────────────────────────┐   │
│  │                      graph_dedup_t (去重层)                         │   │
│  │                                                                      │   │
│  │   SimHash 计算 ──► Bloom Filter 预检 ──► 精确比较                    │   │
│  │   反向映射表：graph_node_id ──► [heap_row_ids]                      │   │
│  └─────────────────────────────────┬─────────────────────────────────┘   │
│                                    │                                        │
│  ┌─────────────────────────────────▼─────────────────────────────────┐   │
│  │                      vector_page_t (页管理层)                      │   │
│  │                                                                      │   │
│  │   内存页池 ──► LRU 置换 ──► 磁盘持久化                               │   │
│  └─────────────────────────────────┬─────────────────────────────────┘   │
│                                    │                                        │
│  ┌─────────────────────────────────▼─────────────────────────────────┐   │
│  │                      heap_storage_t (Heap 存储)                    │   │
│  │                                                                      │   │
│  │   完整记录存储 ──► row_id 分配 ──► 元数据索引                        │   │
│  └─────────────────────────────────────────────────────────────────────┘   │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘
```

## 模块设计

### 1. 向量页管理 (vector_page)

#### 1.1 数据结构

```c
/**
 * @brief 向量页头
 */
typedef struct vector_page_header_s {
    int32_t page_id;           /**< 页 ID */
    int32_t vector_count;      /**< 页内向量数 */
    int32_t capacity;          /**< 页容量 */
    int32_t next_page;         /**< 下一页 ID，-1 表示无 */
    bool dirty;                /**< 是否脏页 */
    uint64_t lsn;              /**< 日志序列号 */
} vector_page_header_t;

/**
 * @brief 向量页
 */
typedef struct vector_page_s {
    vector_page_header_t header;      /**< 页头 */
    uint8_t *data;                   /**< 向量数据区 */
    void *meta;                      /**< 关联元数据 */
} vector_page_t;

/**
 * @brief 页池
 */
typedef struct vector_page_pool_s {
    vector_page_t **pages;           /**< 页数组 */
    int32_t page_count;              /**< 页面数量 */
    int32_t max_pages;               /**< 最大页面数 */
    int32_t page_size;               /**< 页大小 */
    int32_t vector_dim;              /**< 向量维度 */
    
    /* 置换策略 */
    int32_t *lru_chain;              /**< LRU 链表 */
    int32_t clock_hand;              /**< Clock 指针 */
    
    /* 统计 */
    uint64_t hits;                   /**< 缓存命中 */
    uint64_t misses;                  /**< 缓存未命中 */
} vector_page_pool_t;

/**
 * @brief 页操作接口
 */
typedef struct vector_page_ops_s {
    /* 分配/释放 */
    vector_page_t *(*alloc)(vector_page_pool_t *pool);
    void (*release)(vector_page_pool_t *pool, vector_page_t *page);
    
    /* 加载/刷出 */
    int (*load)(vector_page_pool_t *pool, int32_t page_id, vector_page_t *page);
    int (*flush)(vector_page_pool_t *pool, const vector_page_t *page);
    
    /* 置换 */
    int32_t (*evict)(vector_page_pool_t *pool);
} vector_page_ops_t;
```

#### 1.2 关键函数

```c
/**
 * @brief 创建页池
 * @param max_pages 最大页数
 * @param page_size 页大小
 * @param vector_dim 向量维度
 * @return 页池指针
 */
vector_page_pool_t *vector_page_pool_create(int32_t max_pages, 
                                            int32_t page_size,
                                            int32_t vector_dim);

/**
 * @brief 获取向量所在的页
 * @param pool 页池
 * @param vector_id 向量 ID
 * @return 向量页指针
 */
vector_page_t *vector_page_get(vector_page_pool_t *pool, int32_t vector_id);

/**
 * @brief 分配新向量到页
 * @param pool 页池
 * @param vector 向量数据
 * @return 向量 ID，-1 表示失败
 */
int32_t vector_page_append(vector_page_pool_t *pool, const float *vector);

/**
 * @brief 驱逐最少使用页
 * @param pool 页池
 * @return 被驱逐的页 ID
 */
int32_t vector_page_evict(vector_page_pool_t *pool);
```

### 2. 图去重 (graph_dedup)

#### 2.1 数据结构

```c
/**
 * @brief 向量指纹（SimHash 结果）
 */
typedef struct vector_fingerprint_s {
    uint64_t hash;                  /**< SimHash 值 */
    uint32_t bucket;                 /**< 分桶号 */
    float norm;                      /**< 范数值 */
} vector_fingerprint_t;

/**
 * @brief 反向映射条目
 */
typedef struct dedup_reverse_entry_s {
    int32_t graph_node_id;           /**< 图中节点 ID */
    int32_t heap_row_count;          /**< 关联的 heap 行数 */
    int32_t *heap_row_ids;           /**< 关联的 heap 行 ID 数组 */
    int32_t capacity;                /**< 容量 */
} dedup_reverse_entry_t;

/**
 * @brief 图去重器
 */
typedef struct graph_dedup_s {
    /* 哈希存储 */
    vector_fingerprint_t *fingerprints;  /**< SimHash 指纹数组 */
    int32_t count;                       /**< 已去重向量数 */
    int32_t capacity;                     /**< 容量 */
    
    /* Bloom Filter */
    bloom_filter_t *bloom;                /**< Bloom Filter */
    
    /* 反向映射 */
    dedup_reverse_entry_t *reverse_map;  /**< 反向映射表 */
    int32_t reverse_capacity;             /**< 反向映射容量 */
    
    /* 配置 */
    int32_t dims;                        /**< 向量维度 */
    float duplicate_threshold;            /**< 重复阈值（距离） */
} graph_dedup_t;
```

#### 2.2 关键函数

```c
/**
 * @brief 创建去重器
 * @param capacity 预估容量
 * @param dims 向量维度
 * @param fp_rate 假阳性率
 * @return 去重器指针
 */
graph_dedup_t *graph_dedup_create(int32_t capacity, int32_t dims, float fp_rate);

/**
 * @brief 检测向量是否重复
 * @param dedup 去重器
 * @param vector 向量
 * @return true 重复，false 不重复
 */
bool graph_dedup_is_duplicate(graph_dedup_t *dedup, const float *vector);

/**
 * @brief 标记向量已插入
 * @param dedup 去重器
 * @param vector 向量
 * @param graph_node_id 图节点 ID
 * @param heap_row_id Heap 行 ID
 * @return 0 成功，-1 失败
 */
int graph_dedup_mark_inserted(graph_dedup_t *dedup, 
                                const float *vector,
                                int32_t graph_node_id,
                                int32_t heap_row_id);

/**
 * @brief 获取图节点关联的所有 Heap 行
 * @param dedup 去重器
 * @param graph_node_id 图节点 ID
 * @param out_rows 输出行 ID 数组
 * @param max_rows 最大行数
 * @return 实际行数
 */
int graph_dedup_get_heap_rows(graph_dedup_t *dedup,
                               int32_t graph_node_id,
                               int32_t *out_rows,
                               int32_t max_rows);
```

#### 2.3 算法流程

```
graph_dedup_check(vector):
    1. 计算 SimHash
       fingerprint = simhash(vector)
    
    2. Bloom Filter 预检
       if bloom.contains(fingerprint):
           // 可能重复，进入精确检查
       else:
           // 一定不重复，直接返回 false
    
    3. 精确检查
       for each existing_vector:
           if distance(vector, existing_vector) < threshold:
               return true  // 重复
    
    4. Bloom Filter 插入
       bloom.add(fingerprint)
    
    5. 返回 false  // 不重复


graph_dedup_mark_inserted(vector, graph_node_id, heap_row_id):
    1. 计算 fingerprint
    2. 查找/创建反向映射条目
    3. 在反向映射中添加 heap_row_id
```

### 3. 空向量过滤

#### 3.1 过滤规则

```c
/**
 * @brief 检测空向量
 * @param vector 向量
 * @param dim 维度
 * @return true 空向量，false 有效向量
 */
static inline bool is_null_vector(const float *vector, int32_t dim) {
    if (vector == NULL) return true;
    
    // 检查全零
    for (int i = 0; i < dim; i++) {
        if (vector[i] != 0.0f) return false;
    }
    return true;
}

/**
 * @brief 检测无效向量
 * @param vector 向量
 * @param dim 维度
 * @return true 无效，false 有效
 */
static inline bool is_invalid_vector(const float *vector, int32_t dim) {
    if (vector == NULL) return true;
    if (dim <= 0) return true;
    
    // 检查 NaN/Inf
    for (int i = 0; i < dim; i++) {
        float v = vector[i];
        if (isnan(v) || isinf(v)) return true;
    }
    
    return false;
}
```

#### 3.2 插入流程

```
vector_engine_insert(vector, metadata):
    1. 元数据写入 Heap，获取 row_id
       row_id = heap_insert(metadata)  // 向量可为 NULL
    
    2. 空向量检查
       if is_null_vector(vector):
           return row_id  // 仅存储在 Heap，跳过索引
    
    3. 无效向量检查
       if is_invalid_vector(vector):
           return -1  // 拒绝
    
    4. 图去重检查
       if graph_dedup.is_duplicate(vector):
           // 仅存储在 Heap，不建立图连接
           dedup.mark(heap_row_id=row_id)
           return row_id
    
    5. 图索引插入
       graph_node_id = graph.insert(vector)
    
    6. 建立映射
       dedup.mark(graph_node_id, row_id)
    
    return row_id
```

### 4. 搜索流程

```
vector_engine_search(query, k):
    1. 在图索引中搜索
       graph_results = graph.search(query, k * 2)  // 扩大范围
    
    2. 去重处理
       unique_results = []
       seen_node_ids = set()
       for result in graph_results:
           if result.node_id not in seen_node_ids:
               // 获取该图节点关联的所有 Heap 行
               heap_rows = dedup.get_heap_rows(result.node_id)
               for row_id in heap_rows:
                   unique_results.append({
                       row_id: row_id,
                       distance: result.distance
                   })
               seen_node_ids.add(result.node_id)
           if len(unique_results) >= k:
               break
    
    3. 返回 Top-K
       return top_k(unique_results, k)
```

## 持久化设计

### 页文件格式

```
┌─────────────────────────────────────────────────────────────┐
│                     vector_pages.dat                         │
├─────────────────────────────────────────────────────────────┤
│  Header (64 bytes)                                          │
│  ┌────────────────────────────────────────────────────────┐│
│  │ magic: "VPGS" (4)                                       ││
│  │ version (4)                                             ││
│  │ page_size (4)                                           ││
│  │ vector_dim (4)                                          ││
│  │ total_pages (4)                                         ││
│  │ free_page_list (4)                                      ││
│  │ reserved (36)                                           ││
│  └────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│  Page 0                                                     │
│  ┌────────────────────────────────────────────────────────┐│
│  │ page_id: 0 (4)                                          ││
│  │ vector_count: 128 (4)                                   ││
│  │ next_page: -1 (4)                                        ││
│  │ data[page_size - 32 bytes]                              ││
│  └────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│  Page 1                                                     │
│  ┌────────────────────────────────────────────────────────┐│
│  │ ...                                                     ││
│  └────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────┘
```

### 去重状态文件

```
┌─────────────────────────────────────────────────────────────┐
│                     dedup_state.bin                          │
├─────────────────────────────────────────────────────────────┤
│  Header (64 bytes)                                          │
│  ┌────────────────────────────────────────────────────────┐│
│  │ magic: "DDED" (4)                                       ││
│  │ version (4)                                             ││
│  │ count (4)                                               ││
│  │ dims (4)                                                ││
│  │ bloom_size (4)                                          ││
│  │ reserved (44)                                           ││
│  └────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│  SimHash 数据 (count * 8 bytes)                            │
│  ┌────────────────────────────────────────────────────────┐│
│  │ hash[0] (8)                                             ││
│  │ hash[1] (8)                                             ││
│  │ ...                                                     ││
│  └────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│  反向映射数据                                               │
│  ┌────────────────────────────────────────────────────────┐│
│  │ graph_node_id (4)                                       ││
│  │ row_count (4)                                           ││
│  │ row_ids[row_count] (4 each)                            ││
│  └────────────────────────────────────────────────────────┘│
├─────────────────────────────────────────────────────────────┤
│  Bloom Filter 数据                                          │
└─────────────────────────────────────────────────────────────┘
```

## 文件清单

```
新增头文件:
├── include/db/storage/vector/vector_page.h      # 向量页管理 API
└── include/db/storage/vector/graph_dedup.h      # 图去重 API

新增实现:
├── src/db/storage/vector/vector_page.c          # 向量页管理实现
└── src/db/storage/vector/graph_dedup.c          # 图去重实现

更新文件:
├── include/db/storage/vector/vector_engine.h    # 添加去重/过滤支持
├── src/db/storage/vector/vector_engine.c        # 集成去重逻辑
└── src/db/index/vector_index/diskann/diskann.h # 添加去重/过滤接口
```

## 性能考虑

### 内存占用

| 组件 | 内存占用 | 说明 |
|------|----------|------|
| SimHash 数组 | n * 8 bytes | n = 向量数 |
| Bloom Filter | ~1.2 * n bits | 5% 假阳性率 |
| 反向映射 | n * 4 bytes avg | 每个图节点平均关联 |
| LRU 链 | m * 4 bytes | m = 最大页数 |

**总估计**：每百万向量约 10-20 MB

### 搜索延迟

```
原始搜索: O(ef_search * log(n))
去重后搜索: O(ef_search * log(n) + d * k)

d = 每个图节点的平均 Heap 行数
k = 请求的 Top-K

典型值: d ≈ 1.01 (假设 1% 重复), 增加 < 2% 开销
```

## 测试计划

1. **单元测试**
   - `vector_page_test.c` - 页分配、置换、持久化
   - `graph_dedup_test.c` - SimHash、Bloom、去重逻辑
   - `empty_filter_test.c` - NULL/全零向量检测

2. **集成测试**
   - 向量引擎插入 100K 向量（含重复），验证图节点数
   - 搜索验证结果正确性
   - 持久化后恢复验证

3. **性能测试**
   - 吞吐量对比（有无去重）
   - 搜索延迟对比
   - 内存占用对比
