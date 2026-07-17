# 向量存储使用指南

## 概述

向量存储引擎（Vector Engine）专为高维向量数据的存储和相似度搜索设计，支持多种索引类型和优化特性。

## 核心特性

| 特性 | 说明 |
|------|------|
| VecPage 分页存储 | 内存页管理 + Clock-Sweep 置换 |
| PQ 量化压缩 | 乘积量化，8-32x 压缩率 |
| HNSW 索引 | 高效近似最近邻搜索 |
| 多种度量 | L2 距离、余弦相似度、内积 |

## 快速开始

### 1. 初始化引擎

```c
#include "db/storage/vector/vector_engine.h"

// 初始化向量引擎
vector_engine_init("/data/vectors");

// 创建向量表
vector_engine_create("embeddings", 128, METRIC_COSINE);

// 打开向量表
void *vec_db = vector_engine_open("embeddings", ACCESS_MODE_READ_WRITE);
```

### 2. 插入向量

```c
// 插入单个向量
float vector[128];
for (int i = 0; i < 128; i++) {
    vector[i] = (float)rand() / RAND_MAX;
}
vector_engine_insert(vec_db, vector, sizeof(vector));

// 批量插入
float vectors[100][128];
for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 128; j++) {
        vectors[i][j] = (float)rand() / RAND_MAX;
    }
}
vector_engine_batch_insert(vec_db, vectors, 100, sizeof(float) * 128);
```

### 3. 基本搜索

```c
// KNN 搜索
float query[128];
for (int i = 0; i < 128; i++) {
    query[i] = (float)rand() / RAND_MAX;
}

vector_search_results_t results;
int k = 10;
vector_engine_search(vec_db, query, k, &results);

printf("Found %d results:\n", results.count);
for (int i = 0; i < results.count; i++) {
    printf("  ID: %lu, Distance: %.4f\n", 
           results.results[i].id, 
           results.results[i].distance);
}
```

## VecPage 分页存储

VecPage 提供高效的内存页管理，使用 Clock-Sweep 置换算法。

### 启用/配置

```c
// VecPage 默认启用，可通过以下方式配置
// 创建时指定页大小和最大页数
vector_engine_db_t *db = (vector_engine_db_t *)vec_db;
if (db->use_page_pool) {
    printf("VecPage 已启用\n");
    printf("页大小: %u 字节\n", db->page_size);
    printf("最大页数: %u\n", db->max_pages);
}
```

### 手动刷盘

```c
// 刷新所有脏页到磁盘
vector_engine_flush_pages(vec_db);

// 获取统计信息
uint32_t total_pages, used_pages, dirty_pages;
vector_engine_page_stats(vec_db, &total_pages, &used_pages, &dirty_pages);
printf("Pages: %u/%u used, %u dirty\n", used_pages, total_pages, dirty_pages);
```

## PQ 量化压缩

PQ（Product Quantization）将向量分成多个子空间，每个子空间独立量化。

### 启用 PQ

```c
// 启用 PQ 量化
// 参数：子空间数（m）、每位数（k_bits 通常为 8）
int ret = vector_engine_enable_pq(vec_db, 16, 8);
if (ret != 0) {
    fprintf(stderr, "启用 PQ 失败\n");
    return;
}
```

### 训练量化器

```c
// 使用现有数据训练量化器
// 如果数据不足，会使用随机数据补充
ret = vector_engine_train_pq(vec_db, 1000);  // 至少 1000 个向量用于训练
if (ret != 0) {
    fprintf(stderr, "训练 PQ 失败\n");
    return;
}
printf("PQ 训练完成，码书大小: %u\n", 
       ((pq_quantizer_t*)((vector_engine_db_t*)vec_db)->quantizer)->num_centroids);
```

### 保存/加载

```c
// 保存 PQ 量化器到文件
ret = vector_engine_save_pq(vec_db);
if (ret != 0) {
    fprintf(stderr, "保存 PQ 失败\n");
}

// 加载 PQ 量化器
ret = vector_engine_load_pq(vec_db);
if (ret != 0) {
    fprintf(stderr, "加载 PQ 失败\n");
}
```

### 禁用 PQ

```c
// 禁用 PQ，恢复原始向量存储
vector_engine_disable_pq(vec_db);
```

## HNSW 索引

HNSW（Hierarchical Navigable Small World）提供高效的近似最近邻搜索。

### 构建索引

```c
// 构建 HNSW 索引
// 参数：M（每层连接数）、efConstruction（构建时搜索范围）
ret = vector_engine_build_index(vec_db, 16, 200);
if (ret != 0) {
    fprintf(stderr, "构建索引失败\n");
}
```

### HNSW 搜索

```c
// HNSW 搜索
float query[128];
// ... 初始化 query ...

vector_search_results_t hnsw_results;
int ret = vector_engine_search_hnsw(vec_db, query, 10, &hnsw_results);

if (ret == 0) {
    printf("HNSW 搜索找到 %u 个结果:\n", hnsw_results.count);
    for (uint32_t i = 0; i < hnsw_results.count; i++) {
        printf("  ID: %lu, Distance: %.4f\n",
               hnsw_results.results[i].id,
               hnsw_results.results[i].distance);
    }
}
```

### 保存/加载索引

```c
// 保存索引到文件
ret = vector_engine_save_index(vec_db);
if (ret != 0) {
    fprintf(stderr, "保存索引失败\n");
}

// 加载索引
ret = vector_engine_load_index(vec_db);
if (ret != 0) {
    fprintf(stderr, "加载索引失败\n");
}
```

## 完整示例

```c
#include "db/storage/vector/vector_engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main() {
    srand((unsigned)time(NULL));
    
    // 1. 初始化
    vector_engine_init("/tmp/vectors");
    
    // 2. 创建/打开向量表
    vector_engine_create("embeddings", 128, METRIC_COSINE);
    void *vec_db = vector_engine_open("embeddings", ACCESS_MODE_READ_WRITE);
    
    // 3. 启用并训练 PQ
    vector_engine_enable_pq(vec_db, 16, 8);
    vector_engine_train_pq(vec_db, 1000);
    
    // 4. 插入 10000 个向量
    printf("插入向量...\n");
    for (int i = 0; i < 10000; i++) {
        float vec[128];
        for (int j = 0; j < 128; j++) {
            vec[j] = (float)rand() / RAND_MAX;
        }
        vector_engine_insert(vec_db, vec, sizeof(vec));
    }
    
    // 5. 构建 HNSW 索引
    printf("构建索引...\n");
    vector_engine_build_index(vec_db, 16, 200);
    
    // 6. 搜索
    printf("执行搜索...\n");
    float query[128];
    for (int i = 0; i < 128; i++) {
        query[i] = (float)rand() / RAND_MAX;
    }
    
    vector_search_results_t results;
    vector_engine_search_hnsw(vec_db, query, 10, &results);
    
    printf("找到 %u 个最近邻:\n", results.count);
    for (uint32_t i = 0; i < results.count && i < 5; i++) {
        printf("  [%u] ID=%lu, Dist=%.4f\n", 
               i, results.results[i].id, results.results[i].distance);
    }
    
    // 7. 保存
    vector_engine_save_pq(vec_db);
    vector_engine_save_index(vec_db);
    
    // 8. 清理
    vector_engine_close(vec_db);
    vector_engine_shutdown();
    
    return 0;
}
```

## 性能调优

### 内存配置

| 参数 | 说明 | 推荐值 |
|------|------|--------|
| VecPage 页数 | 内存中缓存的页数 | 根据可用内存调整 |
| PQ 子空间数 | 子向量分段数 | 维度/4 到 维度/2 |
| HNSW M | 每层连接数 | 16-64 |
| HNSW ef | 搜索范围 | 50-500 |

### 压缩率对比

| 模式 | 压缩率 | 精度损失 | 适用场景 |
|------|--------|----------|----------|
| 原始 | 1x | 无 | 追求最高精度 |
| PQ-16 | 16x | 5-15% | 内存受限场景 |
| PQ-8 | 8x | 2-8% | 平衡场景 |

## 常见问题

**Q: PQ 训练需要多少数据？**
A: 建议至少 1000 个向量，数据越多码书质量越好。

**Q: 如何选择 HNSW 参数？**
A: M 影响索引大小和精度，ef 影响搜索精度和速度。根据精度要求调整。

**Q: VecPage 和 PQ 可以同时使用吗？**
A: 可以，VecPage 管理内存页，PQ 管理向量压缩，互不冲突。
