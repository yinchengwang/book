# 向量查询执行器设计文档

## 1. 概述

本文档定义 MiniVecDB 向量查询执行器的完整架构，包括查询执行管线、执行器算子、查询计划结构、执行上下文等核心组件。

### 1.1 设计目标

- **统一执行管线**: 整合粗排（HNSW/DiskANN/IVF）、精排（ReRanker）、混合检索（BM25 + Vector）三阶段
- **可扩展算子**: 支持 VectorScan、VectorFilter、VectorLimit、VectorJoin 等算子链式组合
- **高性能执行**: SIMD 加速距离计算、列块数据结构、批量处理
- **灵活配置**: 查询计划缓存、索引动态注册、执行剖析

### 1.2 现状分析

| 组件 | 现有实现 | 需补充内容 |
|------|---------|-----------|
| VectorQueryPlan | 已有粗排/精排/混合配置 | 需补充 PlanNode 算子链结构 |
| VectorQueryResult | 已有 ids/distances/scores | 需补充分页支持 (offset/limit) |
| vector_query_execute | 已有两阶段执行 | 需扩展算子链执行模式 |
| VectorScanState | 已有桩实现 | 需完善实际扫描逻辑 |
| SIMD 距离计算 | 已有 L2/Cosine/Dot | 需集成到执行器算子 |
| 索引注册 | 已有动态注册机制 | 需扩展范围查询接口 |

---

## 2. 查询执行管线架构

### 2.1 执行管线定义

向量查询执行管线采用 **Parse → Plan → Execute → Result** 四阶段模型：

```
┌─────────────────────────────────────────────────────────────────────┐
│                    VectorQueryPipeline                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Stage 1: Parse (查询解析)                                           │
│    │                                                                 │
│    │  输入: VectorQueryRequest (query_vector, k, metric, filter)    │
│    │  输出: QueryParseResult (parsed_params, query_signature)       │
│    │                                                                 │
│    │  - 参数验证 (维度检查、k 值范围、度量类型)                      │
│    │  - 查询签名生成 (用于计划缓存)                                 │
│    │  - 过滤条件解析 (标量过滤表达式)                               │
│    ▼                                                                 │
│  Stage 2: Plan (查询计划)                                            │
│    │                                                                 │
│    │  输入: QueryParseResult                                        │
│    │  输出: VectorQueryPlan (算子链 + 执行配置)                     │
│    │                                                                 │
│    │  - 计划缓存查找 (命中则跳过后续)                               │
│    │  - 索引选择 (HNSW/BM25/混合，基于统计信息)                     │
│    │  - 算子链构建 (Scan → Filter → Limit → Output)                │
│    │  - 参数调优 (ef_search, max_candidates)                        │
│    ▼                                                                 │
│  Stage 3: Execute (执行)                                             │
│    │                                                                 │
│    │  输入: VectorQueryPlan + ExecutionContext                     │
│    │  输出: VectorQueryResult (ids, distances, scores)              │
│    │                                                                 │
│    │  ┌─────────────────────────────────────────────────────────┐  │
│    │  │              Phase 1: 粗排 (Coarse Ranking)              │  │
│    │  │  - VectorScan 算子: 调用底层索引搜索                     │  │
│    │  │  - 返回 Top-N 候选 (N = max_candidates)                 │  │
│    │  └─────────────────────────────────────────────────────────┘  │
│    │    │ 候选集 (N 个)                                            │
│    │    ▼                                                          │
│    │  ┌─────────────────────────────────────────────────────────┐  │
│    │  │              Phase 2: 精排 (Reranking)                   │  │
│    │  │  - VectorFilter 算子: 应用标量过滤条件                   │  │
│    │  │  - ReRanker 精确距离计算                                 │  │
│    │  │  - MMR 去重 (可选)                                       │  │
│    │  └─────────────────────────────────────────────────────────┘  │
│    │    │ 精排后候选集                                             │
│    │    ▼                                                          │
│    │  ┌─────────────────────────────────────────────────────────┐  │
│    │  │              Phase 3: 混合检索 (可选)                    │  │
│    │  │  - BM25 全文检索 (VectorJoin 算子)                       │  │
│    │  │  - RRF / 加权融合                                        │  │
│    │  └─────────────────────────────────────────────────────────┘  │
│    │    │                                                          │
│    │    ▼                                                          │
│    │  ┌─────────────────────────────────────────────────────────┐  │
│    │  │              Phase 4: 输出                               │  │
│    │  │  - VectorLimit 算子: 截取 Top-K                         │  │
│    │  │  - 结果集构建                                            │  │
│    │  └─────────────────────────────────────────────────────────┘  │
│    ▼                                                                 │
│  Stage 4: Result (结果封装)                                          │
│    │                                                                 │
│    │  输入: 执行器原始输出                                          │
│    │  输出: VectorQueryResult (序列化 + 统计)                      │
│    │                                                                 │
│    │  - 结果集构建 (ids, distances, scores)                        │
│    │  - 分页处理 (offset/limit)                                    │
│    │  - 执行统计 (耗时、候选数)                                    │
│    │  - JSON 序列化 (REST API 兼容)                                │
│    ▼                                                                 │
│  输出: VectorQueryResult                                             │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
```

### 2.2 执行模式

支持三种执行模式：

| 模式 | 说明 | 适用场景 |
|------|------|---------|
| `VECTOR_EXEC_BATCH` | 批量处理，一次处理整个批次 | 高吞吐量场景 |
| `VECTOR_EXEC_ITER` | 迭代器模式，逐行处理 | 流式处理、内存受限 |
| `VECTOR_EXEC_HYBRID` | 混合模式，粗排批量 + 精排迭代 | 平衡吞吐与精度 |

---

## 3. 执行器算子定义

### 3.1 算子抽象接口

所有算子继承统一基类 `PlanNode`：

```c
/** 算子类型 */
typedef enum PlanNodeType_e {
    PLAN_NODE_VECTOR_SCAN = 0,    /**< 向量扫描 */
    PLAN_NODE_VECTOR_FILTER = 1,  /**< 向量过滤 */
    PLAN_NODE_VECTOR_LIMIT = 2,   /**< 向量截断 */
    PLAN_NODE_VECTOR_JOIN = 3,    /**< 向量连接 (预留) */
    PLAN_NODE_VECTOR_SORT = 4,    /**< 向量排序 (预留) */
    PLAN_NODE_VECTOR_AGG = 5      /**< 向量聚合 (预留) */
} PlanNodeType;

/** 算子节点基类 */
typedef struct PlanNode_s {
    PlanNodeType type;            /**< 算子类型 */
    struct PlanNode_s *left;      /**< 左子节点 (上游算子) */
    struct PlanNode_s *right;     /**< 右子节点 (下游算子，Join 用) */
    int32_t cost_estimate;        /**< 代价估计 */
    int32_t rows_estimate;        /**< 行数估计 */
    void *private_data;           /**< 算子私有数据 */
} PlanNode;

/** 算子状态基类 */
typedef struct PlanState_s {
    PlanNode *plan;               /**< 对应计划节点 */
    ExecContext *context;         /**< 执行上下文 */
    VectorBatch *batch;           /**< 当前批次 */
    bool done;                    /**< 是否完成 */
    char error_msg[256];          /**< 错误信息 */
} PlanState;

/** 算子执行函数类型 */
typedef VectorBatch *(*plan_exec_fn)(PlanState *state);
```

### 3.2 VectorScan 算子

**功能**: 调用底层索引搜索，返回候选向量批次

```c
/** VectorScan 算子计划 */
typedef struct VectorScanNode_s {
    PlanNode base;
    VectorIndexType index_type;   /**< 索引类型 (HNSW/DiskANN/IVF) */
    int32_t ef_search;            /**< 搜索参数 ef */
    int32_t max_candidates;       /**< 最大候选数 */
    char index_name[64];          /**< 索引名称 (注册表键) */
    DistanceMetric metric;        /**< 距离度量 */
} VectorScanNode;

/** VectorScan 算子状态 */
typedef struct VectorScanState_s {
    PlanState base;
    VectorExecMode mode;          /**< 执行模式 */
    int batch_size;               /**< 批次大小 */
    void *index_impl;             /**< 索引实现指针 */
    vector_index_search_fn search_fn; /**< 搜索函数 */
    vector_index_get_vector_fn get_vec_fn; /**< 获取向量函数 */
    int64_t *candidate_ids;       /**< 候选 ID 缓冲区 */
    float *candidate_dists;       /**< 候选距离缓冲区 */
    int32_t num_candidates;       /**< 候选数量 */
    int32_t current_idx;          /**< 当前处理索引 */
    Expr *filter_expr;            /**< 过滤表达式 (可选) */
} VectorScanState;

/** 执行流程 */
VectorBatch *vector_scan_exec(VectorScanState *state) {
    // 1. 调用索引搜索函数
    int32_t k = state->plan->max_candidates;
    int32_t found = state->search_fn(
        state->index_impl,
        state->context->query_vector,
        k,
        state->candidate_ids,
        state->candidate_dists
    );
    
    // 2. 构建 VectorBatch
    VectorBatch *batch = vector_batch_create(found);
    VectorBlock *block = vector_block_create(found, 3);
    
    // 列 0: ID
    vector_block_set_column(block, 0, state->candidate_ids, sizeof(int64_t));
    // 列 1: 距离
    vector_block_set_column(block, 1, state->candidate_dists, sizeof(float));
    // 列 2: 向量指针 (精排用)
    float **vec_ptrs = malloc(found * sizeof(float *));
    for (int i = 0; i < found; i++) {
        vec_ptrs[i] = state->get_vec_fn(state->index_impl, state->candidate_ids[i]);
    }
    vector_block_set_column(block, 2, vec_ptrs, sizeof(float *));
    
    vector_block_set_num_rows(block, found);
    vector_batch_add_block(batch, block);
    
    state->num_candidates = found;
    state->done = true;
    
    return batch;
}
```

### 3.3 VectorFilter 算子

**功能**: 应用标量过滤条件（元数据过滤）

```c
/** VectorFilter 算子计划 */
typedef struct VectorFilterNode_s {
    PlanNode base;
    Expr *filter_expr;            /**< 过滤表达式树 */
    CompareOp op;                 /**< 比较操作符 */
    int32_t filter_column;        /**< 过滤列索引 */
    void *filter_value;           /**< 过滤值 */
} VectorFilterNode;

/** VectorFilter 算子状态 */
typedef struct VectorFilterState_s {
    PlanState base;
    VectorExecMode mode;
    VectorBatch *input_batch;     /**< 输入批次 (来自上游 Scan) */
    uint64_t *filter_bitmap;      /**< 过滤结果位图 */
    int32_t num_pass;             /**< 通过过滤数量 */
} VectorFilterState;

/** 执行流程 */
VectorBatch *vector_filter_exec(VectorFilterState *state) {
    // 1. 从上游算子获取批次
    VectorBatch *input = plan_exec(state->base.plan->left);
    if (input == NULL) {
        state->done = true;
        return NULL;
    }
    
    // 2. 执行 SIMD 过滤
    VectorBlock *block = input->blocks[0];
    VectorFilterResult *result = vector_filter_execute(
        block,
        state->base.plan->filter_column,
        state->base.plan->filter_value,
        state->base.plan->op
    );
    
    // 3. 构建过滤后批次
    VectorBatch *output = vector_batch_create(result->num_matches);
    VectorBlock *out_block = vector_block_create(result->num_matches, 3);
    
    // 复制匹配行
    for (int i = 0; i < result->num_matches; i++) {
        int64_t row_idx = result->matches[i];
        // 复制列数据...
    }
    
    vector_filter_result_free(result);
    
    return output;
}
```

### 3.4 VectorLimit 算子

**功能**: 截取 Top-K 结果

```c
/** VectorLimit 算子计划 */
typedef struct VectorLimitNode_s {
    PlanNode base;
    int32_t limit;                /**< Top-K */
    int32_t offset;               /**< 偏移量 (分页) */
    bool with_scores;             /**< 是否返回分数 */
} VectorLimitNode;

/** VectorLimit 算子状态 */
typedef struct VectorLimitState_s {
    PlanState base;
    int32_t rows_returned;        /**< 已返回行数 */
    int32_t rows_skipped;         /**< 已跳过行数 (offset) */
    bool sorted;                  /**< 是否已排序 */
} VectorLimitState;

/** 执行流程 */
VectorBatch *vector_limit_exec(VectorLimitState *state) {
    // 1. 从上游算子获取批次
    VectorBatch *input = plan_exec(state->base.plan->left);
    if (input == NULL || state->rows_returned >= state->base.plan->limit) {
        state->done = true;
        return NULL;
    }
    
    // 2. 跳过 offset 行
    if (state->rows_skipped < state->base.plan->offset) {
        int32_t skip = min(input->num_rows, state->base.plan->offset - state->rows_skipped);
        state->rows_skipped += skip;
        // 移除批次前 skip 行...
    }
    
    // 3. 截取剩余行
    int32_t remaining = state->base.plan->limit - state->rows_returned;
    int32_t take = min(input->num_rows, remaining);
    
    VectorBatch *output = vector_batch_create(take);
    // 复制前 take 行...
    
    state->rows_returned += take;
    
    return output;
}
```

### 3.5 VectorJoin 算子 (预留)

**功能**: 混合检索时合并向量检索与 BM25 结果

```c
/** VectorJoin 算子计划 */
typedef struct VectorJoinNode_s {
    PlanNode base;
    VectorFusionType fusion_type; /**< 融合类型 (RRF/加权) */
    float vector_weight;          /**< 向量权重 */
    float text_weight;            /**< 文本权重 */
    int32_t rrf_k;                /**< RRF 参数 k (通常 60) */
} VectorJoinNode;

/** VectorJoin 算子状态 */
typedef struct VectorJoinState_s {
    PlanState base;
    VectorBatch *vector_batch;    /**< 向量检索批次 (左子树) */
    VectorBatch *text_batch;      /**< BM25 批次 (右子树) */
    void *hash_table;             /**< ID → 分数哈希表 */
} VectorJoinState;

/** 执行流程 (RRF 融合) */
VectorBatch *vector_join_exec(VectorJoinState *state) {
    // 1. 获取左右子树批次
    VectorBatch *vec = plan_exec(state->base.plan->left);
    VectorBatch *txt = plan_exec(state->base.plan->right);
    
    // 2. 构建 ID → 分数哈希表
    // 向量检索结果: distance → score 转换
    for (int i = 0; i < vec->num_rows; i++) {
        int64_t id = get_id(vec, i);
        float dist = get_distance(vec, i);
        float score = 1.0f / (state->base.plan->rrf_k + i + 1);
        hash_table_insert(state->hash_table, id, score);
    }
    
    // BM25 结果: 同样计算 RRF 分数
    for (int i = 0; i < txt->num_rows; i++) {
        int64_t id = get_id(txt, i);
        float bm25_score = get_score(txt, i);
        float rrf_score = 1.0f / (state->base.plan->rrf_k + i + 1);
        hash_table_update(state->hash_table, id, rrf_score);
    }
    
    // 3. 按融合分数排序
    VectorBatch *output = hash_table_to_sorted_batch(state->hash_table);
    
    return output;
}
```

---

## 4. 查询计划结构

### 4.1 QueryPlan 扩展

现有 `VectorQueryPlan` 需扩展算子链结构：

```c
/** 扩展后的查询计划 */
typedef struct VectorQueryPlan_s {
    /* === 算子链 (新增) === */
    PlanNode *root_node;          /**< 算子链根节点 */
    int32_t num_nodes;            /**< 算子数量 */
    
    /* === 粗排配置 (已有) === */
    VectorIndexType coarse_index_type;
    int32_t coarse_ef_search;
    int32_t coarse_max_candidates;
    
    /* === 精排配置 (已有) === */
    RerankerType reranker_type;
    RerankerConfig reranker_config;
    
    /* === 混合检索配置 (已有) === */
    bool hybrid_enabled;
    float vector_weight;
    float text_weight;
    VectorFusionType fusion_type;
    
    /* === 输出配置 (扩展) === */
    int32_t top_k;
    int32_t offset;               /**< 分页偏移 (新增) */
    bool with_scores;
    
    /* === 统计信息 (已有) === */
    int64_t plan_id;
    int64_t created_at;
    int64_t hit_count;
} VectorQueryPlan;
```

### 4.2 PlanNode 构建流程

```c
/** 构建简单查询计划 (Scan → Limit) */
VectorQueryPlan *build_simple_plan(VectorIndexType index_type, int32_t k) {
    VectorQueryPlan *plan = vector_query_plan_create();
    
    // 1. 创建 Scan 算子
    VectorScanNode *scan = create_vector_scan_node(index_type, k * 10);
    
    // 2. 创建 Limit 算子
    VectorLimitNode *limit = create_vector_limit_node(k, 0);
    
    // 3. 构建算子链
    limit->base.left = &scan->base;
    plan->root_node = &limit->base;
    plan->num_nodes = 2;
    
    return plan;
}

/** 构建混合查询计划 (VectorScan + BM25 → Join → Limit) */
VectorQueryPlan *build_hybrid_plan(int32_t k) {
    VectorQueryPlan *plan = vector_query_plan_create();
    
    // 1. 创建向量 Scan 算子
    VectorScanNode *vec_scan = create_vector_scan_node(VECTOR_INDEX_HNSW, k * 10);
    
    // 2. 创建 BM25 Scan 算子 (预留)
    PlanNode *bm25_scan = create_bm25_scan_node(k * 10);
    
    // 3. 创建 Join 算子
    VectorJoinNode *join = create_vector_join_node(FUSION_RRF, 60);
    join->base.left = &vec_scan->base;
    join->base.right = bm25_scan;
    
    // 4. 创建 Limit 算子
    VectorLimitNode *limit = create_vector_limit_node(k, 0);
    limit->base.left = &join->base;
    
    // 5. 构建算子链
    plan->root_node = &limit->base;
    plan->num_nodes = 4;
    plan->hybrid_enabled = true;
    
    return plan;
}
```

---

## 5. 执行上下文

### 5.1 ExecutionContext 结构

```c
/** 执行上下文 */
typedef struct ExecContext_s {
    /* === 查询输入 === */
    const float *query_vector;    /**< 查询向量 */
    int32_t query_dims;           /**< 向量维度 */
    DistanceMetric metric;        /**< 距离度量 */
    
    /* === 执行配置 === */
    VectorExecMode exec_mode;     /**< 执行模式 */
    int32_t batch_size;           /**< 批次大小 */
    int32_t timeout_ms;           /**< 超时时间 (毫秒) */
    volatile bool *cancelled;     /**< 取消标志 */
    
    /* === 执行状态 === */
    VectorQueryState state;       /**< 执行状态 */
    int64_t start_time_us;        /**< 开始时间 */
    int64_t current_time_us;      /**< 当前时间 */
    
    /* === 剖析信息 === */
    bool profiling_enabled;       /**< 是否启用剖析 */
    VectorQueryProfileEntry profile_entries[5]; /**< 剖析条目 */
    int32_t profile_idx;          /**< 当前剖析索引 */
    
    /* === 错误处理 === */
    char error_msg[256];          /**< 错误信息 */
    int32_t error_code;           /**< 错误码 */
} ExecContext;

/** 创建执行上下文 */
ExecContext *exec_context_create(const float *query, int32_t dims, DistanceMetric metric) {
    ExecContext *ctx = calloc(1, sizeof(ExecContext));
    ctx->query_vector = query;
    ctx->query_dims = dims;
    ctx->metric = metric;
    ctx->exec_mode = VECTOR_EXEC_BATCH;
    ctx->batch_size = 1024;
    ctx->timeout_ms = 30000;  // 30 秒
    ctx->state = VECTOR_QUERY_READY;
    ctx->start_time_us = get_current_time_us();
    return ctx;
}
```

### 5.2 执行器驱动循环

```c
/** 执行算子链 */
VectorQueryResult *execute_plan(VectorQueryPlan *plan, ExecContext *ctx) {
    VectorBatch *batch = NULL;
    VectorQueryResult *result = NULL;
    
    // 1. 初始化算子状态链
    PlanState *state_chain = init_plan_state_chain(plan->root_node, ctx);
    
    // 2. 执行循环
    while (!ctx->cancelled && !state_chain->done) {
        // 超时检查
        if (ctx->timeout_ms > 0) {
            int64_t elapsed = get_current_time_us() - ctx->start_time_us;
            if (elapsed > ctx->timeout_ms * 1000) {
                snprintf(ctx->error_msg, sizeof(ctx->error_msg), "Query timeout");
                break;
            }
        }
        
        // 执行算子链 (从根节点开始)
        batch = execute_state_chain(state_chain);
        
        if (batch == NULL) break;
        
        // 累积结果
        result = accumulate_result(result, batch);
        
        vector_batch_destroy(batch);
    }
    
    // 3. 清理算子状态
    destroy_plan_state_chain(state_chain);
    
    // 4. 填充统计信息
    if (result != NULL) {
        result->total_time_us = get_current_time_us() - ctx->start_time_us;
        if (ctx->profiling_enabled) {
            copy_profile_entries(result, ctx->profile_entries);
        }
    }
    
    return result;
}
```

---

## 6. 结果集封装

### 6.1 VectorQueryResult 扩展

```c
/** 扩展后的查询结果 */
typedef struct VectorQueryResult_s {
    /* === 结果数据 (已有) === */
    int64_t *ids;                 /**< 向量 ID 数组 */
    float *distances;             /**< 距离数组 */
    float *scores;                /**< 综合分数 (混合检索) */
    int32_t count;                /**< 结果数量 */
    int32_t capacity;             /**< 容量 */
    
    /* === 分页信息 (新增) === */
    int32_t offset;               /**< 偏移量 */
    int32_t total_candidates;     /**< 总候选数 (未分页前) */
    
    /* === 元数据 (新增) === */
    void **metadata;              /**< 元数据数组 (可选) */
    int32_t metadata_count;       /**< 元数据列数 */
    
    /* === 统计信息 (已有) === */
    int64_t coarse_time_us;
    int64_t rerank_time_us;
    int64_t fusion_time_us;       /**< 融合耗时 (新增) */
    int64_t total_time_us;
    int32_t candidates;
    
    /* === 执行剖析 (新增) === */
    VectorQueryProfileEntry profile[5];
    int32_t profile_count;
} VectorQueryResult;
```

### 6.2 结果集构建与序列化

```c
/** 结果集构建 */
VectorQueryResult *build_result_from_batch(VectorBatch *batch, VectorQueryPlan *plan) {
    VectorQueryResult *result = calloc(1, sizeof(VectorQueryResult));
    result->count = batch->num_rows;
    result->capacity = batch->num_rows;
    
    // 分配数组
    result->ids = malloc(result->count * sizeof(int64_t));
    result->distances = malloc(result->count * sizeof(float));
    if (plan->with_scores) {
        result->scores = malloc(result->count * sizeof(float));
    }
    
    // 从批次复制数据
    VectorBlock *block = batch->blocks[0];
    int64_t *id_col = vector_block_get_column(block, 0);
    float *dist_col = vector_block_get_column(block, 1);
    
    memcpy(result->ids, id_col, result->count * sizeof(int64_t));
    memcpy(result->distances, dist_col, result->count * sizeof(float));
    
    if (result->scores) {
        float *score_col = vector_block_get_column(block, 2);
        memcpy(result->scores, score_col, result->count * sizeof(float));
    }
    
    return result;
}

/** JSON 序列化 (REST API 兼容) */
char *vector_query_result_to_json(const VectorQueryResult *result) {
    char *json = malloc(4096 + result->count * 128);
    char *ptr = json;
    
    ptr += sprintf(ptr, "{\"count\":%d,", result->count);
    ptr += sprintf(ptr, "\"total_candidates\":%d,", result->total_candidates);
    ptr += sprintf(ptr, "\"total_time_us\":%ld,", result->total_time_us);
    
    ptr += sprintf(ptr, "\"results\":[");
    for (int i = 0; i < result->count; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");
        ptr += sprintf(ptr, "{\"id\":%ld,\"distance\":%.6f",
                       result->ids[i], result->distances[i]);
        if (result->scores) {
            ptr += sprintf(ptr, ",\"score\":%.6f", result->scores[i]);
        }
        ptr += sprintf(ptr, "}");
    }
    ptr += sprintf(ptr, "]");
    
    ptr += sprintf(ptr, ",\"profile\":{");
    for (int i = 0; i < result->profile_count; i++) {
        if (i > 0) ptr += sprintf(ptr, ",");
        ptr += sprintf(ptr, "\"stage_%d\":%ld",
                       result->profile[i].stage,
                       result->profile[i].duration_us);
    }
    ptr += sprintf(ptr, "}}");
    
    return json;
}
```

---

## 7. 扩展查询接口

### 7.1 范围查询

```c
/**
 * @brief 范围查询：返回距离小于阈值的向量
 */
VectorQueryResult *vector_query_range(const float *query,
                                       int32_t dims,
                                       float max_distance,
                                       int32_t max_results);

/**
 * @brief 多索引查询：同时查询多个索引并融合结果
 */
VectorQueryResult *vector_query_multi_index(const float *query,
                                             int32_t dims,
                                             const char **index_names,
                                             int32_t num_indices,
                                             VectorFusionType fusion_type);
```

### 7.2 过滤查询

```c
/** 过滤表达式类型 */
typedef enum FilterExprType_e {
    FILTER_EXPR_COMPARE = 0,     /**< 比较表达式 */
    FILTER_EXPR_LOGIC = 1,       /**< 逻辑表达式 (AND/OR) */
    FILTER_EXPR_IN = 2           /**< IN 表达式 */
} FilterExprType;

/** 过滤表达式 */
typedef struct FilterExpr_s {
    FilterExprType type;
    int32_t column_idx;
    CompareOp op;
    void *value;
    struct FilterExpr_s *left;   /**< 左子表达式 */
    struct FilterExpr_s *right;  /**< 右子表达式 */
} FilterExpr;

/**
 * @brief 带过滤条件的向量查询
 */
VectorQueryResult *vector_query_filtered(const VectorQueryPlan *plan,
                                          const float *query,
                                          int32_t dims,
                                          const FilterExpr *filter);
```

---

## 8. 性能优化策略

### 8.1 SIMD 加速距离计算

优先使用 AVX2/AVX-512 实现：

```c
#ifdef __AVX2__
float vector_distance_l2_avx2(const float *a, const float *b, int dim) {
    float sum = 0.0f;
    int i = 0;
    
    // AVX2 批量处理 8 个 float
    for (; i + 7 < dim; i += 8) {
        __m256 va = _mm256_loadu_ps(a + i);
        __m256 vb = _mm256_loadu_ps(b + i);
        __m256 diff = _mm256_sub_ps(va, vb);
        __m256 sq = _mm256_mul_ps(diff, diff);
        
        // 水平求和
        __m128 low = _mm256_castps256_ps128(sq);
        __m128 high = _mm256_extractf128_ps(sq, 1);
        __m128 sum128 = _mm_add_ps(low, high);
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum128 = _mm_hadd_ps(sum128, sum128);
        sum += _mm_cvtss_f32(sum128);
    }
    
    // 处理剩余元素
    for (; i < dim; i++) {
        float diff = a[i] - b[i];
        sum += diff * diff;
    }
    
    return sqrtf(sum);
}
#endif
```

### 8.2 查询计划缓存

基于查询签名缓存：

```c
/** 查询签名生成 */
void generate_query_signature(const VectorQueryPlan *plan, int32_t dims, char *sig) {
    snprintf(sig, 128, "%d_%d_%d_%d_%d",
             plan->coarse_index_type,
             plan->coarse_ef_search,
             plan->coarse_max_candidates,
             plan->top_k,
             dims);
}

/** 缓存查找 */
VectorQueryPlan *lookup_plan_cache(const char *sig) {
    for (int i = 0; i < VECTOR_QUERY_PLAN_CACHE_SIZE; i++) {
        if (g_plan_cache[i] && strcmp(g_plan_cache[i]->signature, sig) == 0) {
            g_plan_cache[i]->hit_count++;
            return g_plan_cache[i];
        }
    }
    return NULL;  // 未命中
}
```

### 8.3 执行剖析

启用剖析收集各阶段耗时：

```c
/** 剖析开始 */
void profile_begin(ExecContext *ctx, VectorQueryStage stage) {
    if (!ctx->profiling_enabled) return;
    
    ctx->profile_entries[ctx->profile_idx].stage = stage;
    ctx->profile_entries[ctx->profile_idx].start_us = get_current_time_us();
}

/** 剖析结束 */
void profile_end(ExecContext *ctx, int32_t items_processed) {
    if (!ctx->profiling_enabled) return;
    
    int idx = ctx->profile_idx;
    ctx->profile_entries[idx].end_us = get_current_time_us();
    ctx->profile_entries[idx].duration_us = 
        ctx->profile_entries[idx].end_us - ctx->profile_entries[idx].start_us;
    ctx->profile_entries[idx].items_processed = items_processed;
    
    ctx->profile_idx++;
}
```

---

## 9. 与现有系统集成

### 9.1 与 vector_query.c 集成

现有 `vector_query_execute` 需扩展为算子链模式：

```c
VectorQueryResult *vector_query_execute(const VectorQueryPlan *plan,
                                         const float *query,
                                         int32_t dims) {
    // 1. 查询签名检查缓存
    char sig[128];
    generate_query_signature(plan, dims, sig);
    VectorQueryPlan *cached = lookup_plan_cache(sig);
    if (cached) {
        plan = cached;  // 使用缓存计划
    }
    
    // 2. 创建执行上下文
    ExecContext *ctx = exec_context_create(query, dims, plan->metric);
    ctx->profiling_enabled = vector_query_is_profiling();
    
    // 3. 执行算子链
    VectorQueryResult *result = execute_plan(plan, ctx);
    
    // 4. 清理
    exec_context_destroy(ctx);
    
    // 5. 更新统计
    update_query_stats(result);
    
    return result;
}
```

### 9.2 与 vector_exec.c 集成

SIMD 距离计算函数已实现，直接在 ReRanker 算子中调用：

```c
/** ReRanker 精排算子 */
VectorBatch *rerank_exec(RerankerState *state) {
    VectorBatch *input = state->input_batch;
    VectorBlock *block = input->blocks[0];
    
    // 获取候选向量
    float **vec_ptrs = vector_block_get_column(block, 2);
    int32_t num_candidates = block->num_rows;
    
    // 批量精排距离计算
    float *precise_dists = malloc(num_candidates * sizeof(float));
    for (int i = 0; i < num_candidates; i++) {
        precise_dists[i] = vector_distance_l2_simd(
            state->query_vector,
            vec_ptrs[i],
            state->query_dims
        );
    }
    
    // 更新距离列
    memcpy(vector_block_get_column(block, 1), precise_dists, num_candidates * sizeof(float));
    free(precise_dists);
    
    // 按距离排序
    sort_batch_by_distance(input);
    
    return input;
}
```

---

## 10. 测试策略

### 10.1 单元测试覆盖

| 测试文件 | 覆盖内容 |
|---------|---------|
| `vector_query_test.cpp` | 查询计划生成、算子执行、结果集构建 |
| `vector_exec_test.cpp` | SIMD 距离计算、VectorBlock/Batch 操作 |
| `vector_filter_test.cpp` | SIMD 过滤、过滤条件组合 |

### 10.2 性能基准测试

```c
// 基准测试: 不同候选集大小的吞吐量
void benchmark_query_throughput() {
    int candidate_sizes[] = {100, 500, 1000, 5000};
    
    for (int i = 0; i < 4; i++) {
        int N = candidate_sizes[i];
        int K = 10;
        
        VectorQueryPlan *plan = build_simple_plan(VECTOR_INDEX_HNSW, K);
        plan->coarse_max_candidates = N;
        
        // 执行 1000 次查询
        int64_t total_us = 0;
        for (int j = 0; j < 1000; j++) {
            VectorQueryResult *result = vector_query_execute(plan, query, dims);
            total_us += result->total_time_us;
            vector_query_result_free(result);
        }
        
        printf("N=%d: avg latency=%ld us, throughput=%.2f QPS\n",
               N, total_us / 1000, 1000.0 * 1000000 / total_us);
    }
}
```

---

## 11. 实现路线图

### Phase 1: 架构设计 (本变更)
- [x] Task 1.1: 定义向量查询执行管线
- [x] Task 1.2: 定义执行器算子
- [x] Task 1.3: 定义查询计划结构
- [x] Task 1.4: 编写设计文档

### Phase 2: 查询计划生成
- Task 2.1-2.4: 实现查询解析、计划构建、索引选择、参数调优

### Phase 3: 执行器实现
- Task 3.1-3.4: 实现 VectorScan/VectorFilter/VectorLimit/执行上下文

### Phase 4: 结果集封装
- Task 4.1-4.4: 结果集构建、序列化、分页

### Phase 5: 扩展接口
- Task 5.1-5.4: 范围查询、过滤查询、多索引查询

### Phase 6: 单元测试
- Task 6.1-6.5: 测试覆盖

### Phase 7: 验证
- Task 7.1-7.3: 编译、测试、演示

---

## 12. 参考文献

- PostgreSQL 执行器架构: `src/backend/executor/execMain.c`
- DuckDB 向量化执行: `src/execution/execution_operator.cpp`
- Velox 向量化引擎: `velox/exec/Operator.cpp`