# ReRanker 两阶段精排设计文档

## 1. 概述

ReRanker 是两阶段检索系统中的精排组件，用于在粗排候选结果基础上进行精确排序和多样性优化。

```
┌─────────────────────────────────────────────────────────────┐
│                    两阶段检索                                │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Query ──→ Stage 1: HNSW(k=100) ─→ candidates[]            │
│               │                                              │
│               ▼                                              │
│  Stage 2: ReRanker ─→ Top-K 结果                           │
│    ├─ 精确距离精排（Precise Reranker）                       │
│    ├─ 多度量融合（Multi-Metric Reranker）                   │
│    └─ MMR 去重（MMR Reranker）                              │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

## 2. 架构设计

### 2.1 模块结构

```
src/db/index/vector_index/reranker/
├── reranker.h/c              # 抽象接口和工厂
├── precise_reranker.h/c      # 精确距离精排
├── multi_metric_reranker.h/c # 多度量融合精排
├── mmr_reranker.h/c          # MMR 去重精排
└── two_stage_search.h/c      # 两阶段检索入口
```

### 2.2 类层次

```
Reranker (抽象基类)
├── PreciseReranker    # 精确距离精排
├── MultiMetricReranker # 多度量融合
└── MmrReranker        # MMR 去重
```

## 3. API 接口

### 3.1 创建与销毁

```c
// 创建 ReRanker
RerankerConfig config = reranker_config_default(RERANKER_PRECISE, 128);
Reranker *reranker = reranker_create(RERANKER_PRECISE, &config);

// 销毁 ReRanker
reranker_destroy(reranker);
```

### 3.2 执行重排

```c
int32_t reranker_rerank(Reranker *reranker,
                        const float *query,           // 查询向量
                        const float *candidates,      // 候选向量数组
                        int32_t n_candidates,         // 候选数量
                        int32_t k,                    // 返回数量
                        float *scores,                // 输出分数
                        int32_t *reranked_ids);       // 输出 ID
```

### 3.3 工厂注册

```c
// 注册自定义重排函数
reranker_register("my_reranker", my_rerank_func, user_data);

// 应用已注册的重排器
reranker_apply("my_reranker", query, candidates, n, k, scores, ids);
```

## 4. 精排器类型

### 4.1 精确距离精排 (Precise Reranker)

使用原始向量计算精确距离（L2/Cosine/IP）进行重排。

**配置**：
```c
config.precise.use_simd = true;  // 是否使用 SIMD
config.metric = DISTANCE_METRIC_L2_SQUARED;
```

**适用场景**：
- Stage 1 使用 PQ 量化的索引
- 需要精确召回的场景

### 4.2 多度量融合 (Multi-Metric Reranker)

结合多种距离度量进行加权融合。

**配置**：
```c
config.multi_metric.num_metrics = 3;
config.multi_metric.metrics[0] = DISTANCE_METRIC_L2_SQUARED;
config.multi_metric.metrics[1] = DISTANCE_METRIC_COSINE;
config.multi_metric.metrics[2] = DISTANCE_METRIC_INNER_PRODUCT;
config.multi_metric.weights[0] = 0.4f;
config.multi_metric.weights[1] = 0.3f;
config.multi_metric.weights[2] = 0.3f;
```

**融合公式**：
```
score = Σ(weight[i] * normalize(distance[i]))
```

### 4.3 MMR 去重 (MMR Reranker)

最大边际相关性（Maximal Marginal Relevance）重排。

**配置**：
```c
config.mmr.lambda = 0.5f;  // [0,1]，控制相关性/多样性平衡
```

**MMR 公式**：
```
MMR = λ * rel(query, doc) - (1-λ) * max_sim(doc, selected_docs)
```

**参数说明**：
- λ = 1.0：纯相关性（与传统排序相同）
- λ = 0.0：纯多样性
- λ = 0.5：平衡模式

## 5. 两阶段检索

### 5.1 创建两阶段检索器

```c
TwoStageConfig config = two_stage_config_default(dims);
config.stage1.k = 100;           // 粗排候选数
config.stage2.reranker_type = RERANKER_MMR;
config.stage2.enable_mmr = true;
config.stage2.mmr_lambda = 0.5f;

TwoStageSearcher *searcher = two_stage_searcher_create(hnsw_index, &config);
```

### 5.2 执行两阶段检索

```c
TwoStageResult results[20];
TwoStageStats stats;

int32_t count = two_stage_search(searcher, query, 10, results, &stats);

printf("Stage 1: %lld us, Stage 2: %lld us\n",
       (long long)stats.stage1_time_us,
       (long long)stats.stage2_time_us);
```

## 6. 性能指标

| 指标 | 目标 |
|------|------|
| Recall@10 | ≥ 95% |
| Stage 2 延迟 | < 5ms（100 候选） |
| MMR 多样性 | 覆盖 ≥ 3 个语义簇 |

## 7. 使用示例

```c
#include "db/index/vector_index/reranker/reranker.h"
#include "db/index/vector_index/reranker/two_stage_search.h"

int main() {
    // 创建精排器
    RerankerConfig config = reranker_config_default(RERANKER_MMR, 128);
    config.mmr.lambda = 0.5f;
    Reranker *reranker = reranker_create(RERANKER_MMR, &config);

    // 准备数据
    float query[128];
    float candidates[100][128];
    float scores[10];
    int32_t ids[10];

    // 生成/加载数据...

    // 执行重排
    int32_t count = reranker_rerank(reranker, query, (float *)candidates,
                                     100, 10, scores, ids);

    // 输出结果
    for (int i = 0; i < count; i++) {
        printf("ID: %d, Score: %.4f\n", ids[i], scores[i]);
    }

    reranker_destroy(reranker);
    return 0;
}
```

## 8. 扩展开发

### 8.1 添加新的精排算法

1. 创建 `my_reranker.h/c`
2. 实现 `my_reranker_create/destroy/rerank` 函数
3. 在 `reranker.c` 的 `reranker_create` 中添加 case
4. 在 `reranker.h` 的 `RerankerType` 枚举中添加新类型

### 8.2 使用自定义重排函数

```c
int32_t my_custom_rerank(const float *query,
                         const float *candidates,
                         int32_t n,
                         int32_t k,
                         float *scores,
                         int32_t *ids,
                         void *user_data) {
    // 自定义重排逻辑
    // ...
}

// 注册
reranker_register("custom", my_custom_rerank, user_data);
```
