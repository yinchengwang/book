# 多模查询架构设计文档

## 概述

本文档定义 MiniVecDB 多模查询架构的完整设计方案，涵盖查询模型、过滤语法、混合检索策略和接口定义。

### 1.1 设计目标

- **统一查询模型**：支持向量检索 + 标量过滤 + 全文检索的统一请求/响应结构
- **灵活过滤语法**：提供用户友好且易于解析的标量过滤表达式
- **高效混合检索**：向量检索与 BM25 全文检索的融合策略
- **标准化接口**：C SDK + REST API + CLI 三层接口规范

### 1.2 现状分析

| 组件 | 现有实现 | 需补充内容 |
|------|---------|-----------|
| 向量查询执行器 | `docs/vector-query-executor.md` 已完成设计 | 需补充多模查询模型 |
| RAG 混合检索 | `enhanced_retriever.h` 支持向量+重排序 | 需补充向量+BM25 融合 |
| 标量过滤 | 仅有 FilterExpr 结构定义 | 需补充完整语法和执行器 |
| C SDK | `vdb_api.h`/`vdb_query.h` 预声明 | 需补充查询接口 |

### 1.3 术语表

| 术语 | 定义 |
|------|------|
| 多模查询 | 同时使用多种检索模式（向量+标量+全文）的查询 |
| 标量过滤 | 基于元数据字段的条件过滤 |
| 混合检索 | 向量检索结果与 BM25 检索结果的融合 |
| RRF | Reciprocal Rank Fusion，互惠排名融合算法 |
| BM25 | 经典全文检索算法，Elasticsearch/Milvus 底层使用 |

---

## 2. 查询模型设计

### 2.1 VectorQueryRequest — 统一查询请求结构

```c
/**
 * @brief 查询模式枚举
 */
typedef enum VectorQueryMode_e {
    VECTOR_QUERY_MODE_VECTOR = 0,      /**< 仅向量检索 */
    VECTOR_QUERY_MODE_FILTERED = 1,    /**< 向量检索 + 标量过滤 */
    VECTOR_QUERY_MODE_HYBRID = 2,      /**< 混合检索（向量 + 全文）*/
    VECTOR_QUERY_MODE_MULTIMODAL = 3   /**< 多模态检索（向量 + 标量 + 全文）*/
} VectorQueryMode;

/**
 * @brief 距离度量类型
 */
typedef enum DistanceMetric_e {
    DISTANCE_METRIC_L2 = 0,            /**< 欧氏距离 */
    DISTANCE_METRIC_COSINE = 1,        /**< 余弦距离 */
    DISTANCE_METRIC_DOT = 2            /**< 点积 */
} DistanceMetric;

/**
 * @brief 统一查询请求结构
 */
typedef struct VectorQueryRequest_s {
    /* === 查询模式 === */
    VectorQueryMode mode;               /**< 查询模式 */
    char collection_name[64];           /**< 集合名称 */

    /* === 向量查询字段 === */
    const float *query_vector;          /**< 查询向量（可为 NULL 如果用 text 查询）*/
    int32_t query_dims;                 /**< 向量维度 */
    int32_t top_k;                      /**< 返回结果数 */
    DistanceMetric metric;              /**< 距离度量 */

    /* === 标量过滤字段 === */
    const char *filter_expression;      /**< 过滤表达式（字符串，语法见第 3 节）*/
    void *filter_ast;                   /**< 过滤表达式 AST（解析后填充）*/

    /* === 全文检索字段 === */
    const char *query_text;             /**< 查询文本（可为 NULL 如果用 vector 查询）*/
    float bm25_weight;                  /**< BM25 权重 (0.0-1.0) */

    /* === 混合控制字段 === */
    float vector_weight;                /**< 向量权重 (0.0-1.0) */
    bool enable_rerank;                 /**< 是否启用重排序 */
    bool enable_mmr;                    /**< 是否启用 MMR 多样性 */
    float mmr_lambda;                   /**< MMR lambda (0=相关性, 1=多样性) */

    /* === 分页字段 === */
    int32_t offset;                     /**< 偏移量 */
    int32_t limit;                      /**< 限制数量（覆盖 top_k 的部分语义）*/

    /* === 执行配置 === */
    int32_t ef_search;                  /**< HNSW 搜索参数 ef */
    int32_t max_candidates;             /**< 最大候选数 */
    int32_t timeout_ms;                 /**< 超时时间（毫秒）*/
    bool with_scores;                   /**< 是否返回分数 */
    bool with_metadata;                 /**< 是否返回元数据 */

    /* === 剖析配置 === */
    bool profiling;                     /**< 是否启用剖析 */
} VectorQueryRequest;
```

### 2.2 VectorQueryResult — 统一查询响应结构

```c
/**
 * @brief 单条查询结果
 */
typedef struct VectorQueryItem_s {
    int64_t id;                         /**< 向量 ID */
    float distance;                     /**< 距离值 */
    float score;                        /**< 综合分数（混合检索时使用）*/
    void *metadata;                     /**< 元数据（JSON 字符串或结构化数据）*/
} VectorQueryItem;

/**
 * @brief 查询统计信息
 */
typedef struct VectorQueryStats_s {
    int64_t parse_time_us;              /**< 解析耗时 */
    int64_t plan_time_us;               /**< 计划生成耗时 */
    int64_t execute_time_us;            /**< 执行耗时 */
    int64_t total_time_us;              /**< 总耗时 */
    int32_t candidates;                 /**< 候选数 */
    int32_t total_before_limit;         /**< 限制前总数 */
    int32_t total_after_limit;          /**< 限制后总数 */
    char index_used[64];                /**< 使用的索引 */
    char fusion_strategy[32];           /**< 融合策略 */
    float fusion_score_details;         /**< 融合分数详情（RRF k 值等）*/
} VectorQueryStats;

/**
 * @brief 查询错误信息
 */
typedef struct VectorQueryError_s {
    int32_t code;                       /**< 错误码 */
    char message[256];                  /**< 错误信息 */
    char stage[32];                     /**< 错误发生阶段 */
} VectorQueryError;

/**
 * @brief 统一查询响应结构
 */
typedef struct VectorQueryResponse_s {
    /* === 结果数据 === */
    VectorQueryItem *items;             /**< 结果数组 */
    int32_t count;                      /**< 结果数量 */
    int32_t capacity;                   /**< 容量 */

    /* === 分页信息 === */
    int32_t offset;                     /**< 请求的偏移量 */
    int32_t limit;                      /**< 请求的限制数量 */
    int32_t total;                      /**< 符合条件的总数 */

    /* === 统计信息 === */
    VectorQueryStats stats;             /**< 查询统计 */

    /* === 剖析信息 === */
    void *profile;                      /**< 剖析数据（可选）*/

    /* === 错误信息 === */
    VectorQueryError error;             /**< 错误信息（无错误时 code=0）*/

    /* === 版本信息 === */
    char api_version[16];               /**< API 版本 */
} VectorQueryResponse;
```

### 2.3 查询模型流程图

```
┌─────────────────────────────────────────────────────────────────────┐
│                     VectorQueryRequest                               │
├─────────────────────────────────────────────────────────────────────┤
│  mode: MULTIMODAL                                                   │
│  collection_name: "products"                                        │
│  query_vector: [0.1, 0.2, ...]     // 向量检索输入                    │
│  query_text: "手机 5G"              // 全文检索输入                    │
│  filter_expression: "price < 5000"  // 标量过滤                      │
│  top_k: 10                                                         │
│  metric: COSINE                                                     │
│  vector_weight: 0.7                                                 │
│  bm25_weight: 0.3                                                   │
│  enable_rerank: true                                                │
│  enable_mmr: false                                                  │
│  ...                                                                │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Query Parser                                      │
│  1. 解析 filter_expression → filter_ast                             │
│  2. 验证 query_vector 维度                                          │
│  3. 验证 query_text 编码                                            │
│  4. 生成查询签名（用于计划缓存）                                      │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Query Planner                                     │
│  1. 查找计划缓存（命中则跳过）                                        │
│  2. 选择索引（HNSW + BM25）                                         │
│  3. 构建算子链：VectorScan + Filter + BM25Scan + Join + Limit        │
│  4. 参数调优（ef_search, max_candidates）                            │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    Query Executor                                    │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  Phase 1: 向量检索 (HNSW/DiskANN/IVF)                          │ │
│  │  - 返回 top_k * 10 候选                                         │ │
│  │  - candidates: 100                                              │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                              │                                       │
│                              ▼                                       │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  Phase 2: 标量过滤 (VectorFilter)                               │ │
│  │  - 解析 filter_ast，执行过滤                                    │ │
│  │  - candidates: 50 (过滤掉 50%)                                  │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                              │                                       │
│                              ▼                                       │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  Phase 3: BM25 全文检索 (如果 query_text != NULL)               │ │
│  │  - 返回 top_k * 10 候选                                         │ │
│  │  - candidates: 100                                              │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                              │                                       │
│                              ▼                                       │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  Phase 4: 混合融合 (VectorJoin - RRF/Weighted)                  │ │
│  │  - 合并向量检索和 BM25 结果                                      │ │
│  │  - candidates: 100 → 30                                         │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                              │                                       │
│                              ▼                                       │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  Phase 5: 重排序 (可选 - Cross-Encoder)                         │ │
│  │  - 使用更精确的模型重新评分                                      │ │
│  │  - candidates: 30 → 10                                          │ │
│  └────────────────────────────────────────────────────────────────┘ │
│                              │                                       │
│                              ▼                                       │
│  ┌────────────────────────────────────────────────────────────────┐ │
│  │  Phase 6: 结果封装                                              │ │
│  │  - 截取 top_k 结果                                              │
│  │  - 构建 VectorQueryResponse                                     │ │
│  └────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                    VectorQueryResponse                               │
├─────────────────────────────────────────────────────────────────────┤
│  items: [                                                          │
│    {id: 123, distance: 0.15, score: 0.92, metadata: {...}},         │
│    {id: 456, distance: 0.23, score: 0.88, metadata: {...}},         │
│    ...                                                              │
│  ]                                                                  │
│  count: 10                                                          │
│  stats: {                                                           │
│    parse_time_us: 120,                                              │
│    plan_time_us: 80,                                                │
│    execute_time_us: 15230,                                          │
│    total_time_us: 15430,                                            │
│    candidates: 100,                                                 │
│    index_used: "hnsw",                                              │
│    fusion_strategy: "rrf"                                           │
│  }                                                                  │
│  error: {code: 0, message: ""}                                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. 标量过滤语法设计

### 3.1 过滤表达式语法选择

经过调研业界主流方案（Milvus 类 SQL、Qdrant MongoDB 风格、Pinecone MongoDB 风格、Weaviate GraphQL），**我们选择类 SQL WHERE 子句风格的字符串表达式**，原因如下：

| 评估维度 | 类 SQL | 结构化 JSON | MongoDB 风格 |
|---------|--------|------------|-------------|
| 用户友好度 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ |
| 解析复杂度 | ⭐⭐⭐（需词法/语法分析）| ⭐⭐⭐⭐⭐（JSON 解析）| ⭐⭐⭐⭐ |
| 表达丰富度 | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| 类型安全 | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ |
| 生态熟悉度 | ⭐⭐⭐⭐⭐（SQL 广泛使用）| ⭐⭐⭐ | ⭐⭐⭐⭐ |

**决策**：采用类 SQL 布尔表达式，理由：
1. 用户群体最熟悉 SQL WHERE 语法
2. 表达能力强，支持复杂嵌套条件
3. 便于未来扩展为完整 SQL 子集
4. 与 PostgreSQL 生态兼容

### 3.2 过滤表达式语法

#### 3.2.1 支持的操作符

| 类别 | 操作符 | 说明 | 示例 |
|------|--------|------|------|
| 比较 | `=` / `==` | 等于 | `category = 'electronics'` |
| 比较 | `!=` / `<>` | 不等于 | `status != 'deleted'` |
| 比较 | `<` | 小于 | `price < 100` |
| 比较 | `<=` | 小于等于 | `price <= 100` |
| 比较 | `>` | 大于 | `price > 50` |
| 比较 | `>=` | 大于等于 | `price >= 50` |
| 字符串 | `LIKE` | 模糊匹配 | `name LIKE '%手机%'` |
| 字符串 | `ILIKE` | 大小写不敏感匹配 | `name ILIKE '%phone%'` |
| 集合 | `IN` | 在集合中 | `category IN ('a', 'b', 'c')` |
| 集合 | `NOT IN` | 不在集合中 | `status NOT IN ('x', 'y')` |
| 存在 | `IS NULL` | 为空 | `description IS NULL` |
| 存在 | `IS NOT NULL` | 非空 | `description IS NOT NULL` |
| 逻辑 | `AND` / `&&` | 逻辑与 | `price < 100 AND category = 'a'` |
| 逻辑 | `OR` / `||` | 逻辑或 | `price < 100 OR price > 1000` |
| 逻辑 | `NOT` / `!` | 逻辑非 | `NOT status = 'deleted'` |
| 分组 | `(` `)` | 优先级分组 | `(a = 1 OR b = 2) AND c = 3` |

#### 3.2.2 语法规则

```
<expr>       ::= <or_expr>
<or_expr>    ::= <and_expr> (('OR' | '||') <and_expr>)*
<and_expr>   ::= <not_expr> (('AND' | '&&') <not_expr>)*
<not_expr>   ::= ('NOT' | '!')? <primary_expr>
<primary_expr> ::= <comparison> | '(' <expr> ')'
<comparison> ::= <value> <op> <value> | <value> 'IS' 'NULL' | <value> 'IS' 'NOT' 'NULL'
<value>      ::= <field> | <literal>
<field>      ::= IDENTIFIER ('.' IDENTIFIER)*
<literal>    ::= NUMBER | STRING | 'NULL' | 'TRUE' | 'FALSE'
<op>         ::= '=' | '==' | '!=' | '<>' | '<' | '<=' | '>' | '>='
                | 'LIKE' | 'ILIKE' | 'IN' | 'NOT' 'IN'
```

#### 3.2.3 示例

```c
// 简单比较
"price < 100"
"category = 'electronics'"
"status != 'deleted'"

// 逻辑组合
"price < 100 AND category = 'electronics'"
"price < 100 OR price > 1000"
"NOT status = 'deleted'"

// 嵌套分组
"(price < 100 OR price > 1000) AND category = 'electronics'"
"(a = 1 OR b = 2) AND (c = 3 OR d = 4)"

// 集合操作
"category IN ('electronics', 'books', 'food')"
"status NOT IN ('deleted', 'archived')"

// 字符串匹配
"name LIKE '%手机%'"
"description ILIKE '%phone%'"

// 空值检查
"description IS NULL"
"description IS NOT NULL"

// 复杂示例
"(price BETWEEN 100 AND 500) AND (category = 'electronics' OR category = 'books') AND status != 'deleted'"
```

### 3.3 过滤表达式 AST 结构

```c
/**
 * @brief 过滤表达式类型
 */
typedef enum FilterExprType_e {
    FILTER_EXPR_COMPARISON = 0,     /**< 比较表达式 */
    FILTER_EXPR_LOGICAL_AND = 1,    /**< 逻辑与 */
    FILTER_EXPR_LOGICAL_OR = 2,     /**< 逻辑或 */
    FILTER_EXPR_LOGICAL_NOT = 3,    /**< 逻辑非 */
    FILTER_EXPR_IN = 4,             /**< IN 表达式 */
    FILTER_EXPR_BETWEEN = 5,        /**< BETWEEN 表达式 */
    FILTER_EXPR_LIKE = 6,           /**< LIKE 表达式 */
    FILTER_EXPR_IS_NULL = 7,        /**< IS NULL 表达式 */
    FILTER_EXPR_GROUP = 8           /**< 分组表达式 */
} FilterExprType;

/**
 * @brief 比较操作符
 */
typedef enum FilterCompareOp_e {
    FILTER_OP_EQ = 0,               /**< = */
    FILTER_OP_NE = 1,               /**< != */
    FILTER_OP_LT = 2,               /**< < */
    FILTER_OP_LE = 3,               /**< <= */
    FILTER_OP_GT = 4,               /**< > */
    FILTER_OP_GE = 5,               /**< >= */
    FILTER_OP_LIKE = 6,             /**< LIKE */
    FILTER_OP_ILIKE = 7             /**< ILIKE (case-insensitive) */
} FilterCompareOp;

/**
 * @brief 字面量类型
 */
typedef enum FilterLiteralType_e {
    FILTER_LITERAL_INT = 0,         /**< 整数 */
    FILTER_LITERAL_FLOAT = 1,       /**< 浮点数 */
    FILTER_LITERAL_STRING = 2,      /**< 字符串 */
    FILTER_LITERAL_BOOL = 3,        /**< 布尔值 */
    FILTER_LITERAL_NULL = 4         /**< NULL */
} FilterLiteralType;

/**
 * @brief 字面量值
 */
typedef struct FilterLiteral_s {
    FilterLiteralType type;
    union {
        int64_t int_val;
        double float_val;
        char *str_val;
        bool bool_val;
    };
} FilterLiteral;

/**
 * @brief 字段引用
 */
typedef struct FilterField_s {
    char *name;                     /**< 字段名 */
    char **path_parts;              /**< 路径部分（嵌套字段）*/
    int32_t num_parts;              /**< 路径深度 */
} FilterField;

/**
 * @brief 过滤表达式节点
 */
typedef struct FilterExprNode_s {
    FilterExprType type;
    FilterCompareOp op;             /**< 比较操作符（用于比较表达式）*/

    /* 字段引用 */
    FilterField *field;             /**< 左操作数字段（可为 NULL）*/

    /* 字面量值 */
    FilterLiteral *value;           /**< 右操作数字面量（可为 NULL）*/
    FilterLiteral *value2;          /**< BETWEEN 的第二个值（可为 NULL）*/
    FilterLiteral **in_values;      /**< IN 表达式的值列表 */
    int32_t num_in_values;          /**< IN 表达式的值数量 */

    /* 子表达式（用于逻辑操作符）*/
    struct FilterExprNode_s *left;  /**< 左子表达式 */
    struct FilterExprNode_s *right; /**< 右子表达式 */

    /* AST 位置信息（用于错误报告）*/
    int line;
    int col;
} FilterExprNode;
```

### 3.4 过滤表达式解析器接口

```c
/**
 * @brief 解析过滤表达式字符串为 AST
 * @param expr 表达式字符串
 * @param error 错误信息输出（可为 NULL）
 * @return 解析后的 AST，失败返回 NULL
 */
FilterExprNode *filter_expr_parse(const char *expr, char *error);

/**
 * @brief 释放过滤表达式 AST
 */
void filter_expr_free(FilterExprNode *node);

/**
 * @brief 打印 AST（用于调试）
 */
void filter_expr_print(const FilterExprNode *node, char *buf, size_t bufsize);

/**
 * @brief 估算 AST 复杂度（用于优化决策）
 */
int32_t filter_expr_estimate_complexity(const FilterExprNode *node);

/**
 * @brief 判断表达式是否可以下推到索引层
 */
bool filter_expr_can_pushdown(const FilterExprNode *node, VectorIndexType index_type);
```

### 3.5 过滤执行器接口

```c
/**
 * @brief 过滤执行上下文
 */
typedef struct FilterExecutor_s {
    FilterExprNode *root;           /**< AST 根节点 */
    VectorMetadataReader *reader;   /**< 元数据读取器 */
    int64_t num_vectors;            /**< 向量总数 */
    void *runtime_state;            /**< 运行时状态 */
} FilterExecutor;

/**
 * @brief 创建过滤执行器
 */
FilterExecutor *filter_executor_create(FilterExprNode *root,
                                       VectorMetadataReader *reader);

/**
 * @brief 执行过滤，返回匹配 ID 列表
 * @param exec 执行器
 * @param candidate_ids 候选 ID 数组
 * @param num_candidates 候选数量
 * @param result_ids 输出结果 ID 数组（调用方分配）
 * @return 匹配数量
 */
int32_t filter_executor_execute(FilterExecutor *exec,
                                 const int64_t *candidate_ids,
                                 int32_t num_candidates,
                                 int64_t *result_ids);

/**
 * @brief 批量执行过滤（SIMD 优化）
 */
int32_t filter_executor_execute_batch(FilterExecutor *exec,
                                       int64_t **id_arrays,
                                       int32_t *array_sizes,
                                       int32_t num_arrays);

/**
 * @brief 销毁过滤执行器
 */
void filter_executor_destroy(FilterExecutor *exec);
```

---

## 4. 混合检索设计

### 4.1 融合策略接口

```c
/**
 * @brief 融合策略类型
 */
typedef enum FusionStrategy_e {
    FUSION_STRATEGY_RRF = 0,        /**< Reciprocal Rank Fusion */
    FUSION_STRATEGY_WEIGHTED = 1,   /**< 加权求和 */
    FUSION_STRATEGY_RERANK = 2,     /**< 重排序融合 */
    FUSION_STRATEGY_COMPARATIVE = 3 /**< Comparative scoring */
} FusionStrategy;

/**
 * @brief 融合结果条目
 */
typedef struct FusionItem_s {
    int64_t id;                     /**< 向量 ID */
    float vector_score;             /**< 向量检索分数 */
    float bm25_score;               /**< BM25 分数 */
    float fused_score;              /**< 融合分数 */
    int vector_rank;                /**< 向量检索排名 */
    int bm25_rank;                  /**< BM25 排名 */
} FusionItem;

/**
 * @brief 融合上下文
 */
typedef struct FusionContext_s {
    FusionStrategy strategy;        /**< 融合策略 */
    float vector_weight;            /**< 向量权重 */
    float bm25_weight;              /**< BM25 权重 */
    int32_t rrf_k;                  /**< RRF 参数 k */
    FusionItem *items;              /**< 融合条目数组 */
    int32_t num_items;              /**< 条目数量 */
    int32_t capacity;               /**< 容量 */
} FusionContext;

/**
 * @brief 创建融合上下文
 */
FusionContext *fusion_context_create(FusionStrategy strategy,
                                      float vector_weight,
                                      float bm25_weight);

/**
 * @brief 添加向量检索结果
 */
void fusion_add_vector_result(FusionContext *ctx,
                               const int64_t *ids,
                               const float *distances,
                               int32_t count);

/**
 * @brief 添加 BM25 检索结果
 */
void fusion_add_bm25_result(FusionContext *ctx,
                             const int64_t *ids,
                             const float *scores,
                             int32_t count);

/**
 * @brief 执行融合，返回排序后的结果
 * @param ctx 融合上下文
 * @param top_k 返回前 top_k 结果
 * @param result_ids 输出结果 ID 数组
 * @param result_scores 输出结果分数数组
 * @return 结果数量
 */
int32_t fusion_execute(FusionContext *ctx,
                        int32_t top_k,
                        int64_t *result_ids,
                        float *result_scores);

/**
 * @brief 销毁融合上下文
 */
void fusion_context_destroy(FusionContext *ctx);
```

### 4.2 RRF（Reciprocal Rank Fusion）算法

RRF 是一种无需调参的排名融合算法，定义如下：

```
RRF_score(d) = Σ 1 / (k + rank(d))
```

其中：
- `d` 是文档/向量 ID
- `k` 是常量参数（通常取 60）
- `rank(d)` 是文档在第 i 个检索结果列表中的排名（从 1 开始）

```c
/**
 * @brief RRF 融合算法
 *
 * 计算公式: RRF_score(d) = Σ 1 / (k + rank_i(d))
 *
 * @param ctx 融合上下文
 * @param top_k 返回前 top_k 结果
 * @param result_ids 输出结果 ID 数组
 * @param result_scores 输出结果分数数组
 * @return 结果数量
 */
int32_t fusion_rrf(FusionContext *ctx,
                    int32_t top_k,
                    int64_t *result_ids,
                    float *result_scores) {
    // 1. 构建 ID → 分数映射
    // ...

    // 2. 对每个结果应用 RRF 公式
    // for each list i:
    //   for each doc d at rank r:
    //     rrf_score[d] += 1.0 / (k + r)

    // 3. 按 RRF 分数排序
    // ...

    // 4. 返回 top_k
}
```

### 4.3 加权求和融合

```c
/**
 * @brief 加权求和融合算法
 *
 * 计算公式: score(d) = vector_weight * norm_vector_score(d) +
 *                      bm25_weight * norm_bm25_score(d)
 *
 * 需要对分数进行归一化（Min-Max 或 Rank-based）
 */
int32_t fusion_weighted_sum(FusionContext *ctx,
                             int32_t top_k,
                             int64_t *result_ids,
                             float *result_scores);
```

### 4.4 混合查询执行流程

```
┌─────────────────────────────────────────────────────────────────────┐
│                    Hybrid Query Execution                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Input:                                                              │
│    - query_vector: [0.1, 0.2, ...]                                   │
│    - query_text: "手机 5G"                                           │
│    - vector_weight: 0.7                                              │
│    - bm25_weight: 0.3                                                │
│    - top_k: 10                                                       │
│                                                                      │
└─────────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┴─────────────┐
                ▼                           ▼
┌───────────────────────────┐   ┌───────────────────────────┐
│   Vector Search (HNSW)    │   │   BM25 Search             │
│   ────────────────────    │   │   ────────────────────    │
│   输入: query_vector       │   │   输入: query_text        │
│   输出: Top-K 向量结果     │   │   输出: Top-K BM25 结果   │
│   {                       │   │   {                       │
│     id: 123, dist: 0.15,  │   │     id: 456, score: 12.5, │
│     id: 789, dist: 0.22,  │   │     id: 123, score: 8.3,  │
│     ...                   │   │     ...                   │
│   }                       │   │   }                       │
└───────────────────────────┘   └───────────────────────────┘
                │                           │
                └─────────────┬─────────────┘
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Fusion Stage                                     │
│   ────────────────────                                                │
│   1. 合并两个结果集（ID 去重）                                        │
│   2. 应用 RRF 或加权融合                                              │
│   3. 按融合分数排序                                                   │
│   4. 返回 Top-K                                                       │
│                                                                      │
│   示例（RRF, k=60）:                                                  │
│     id=123: RRF = 1/(60+1) + 1/(60+2) = 0.0164 + 0.0161 = 0.0325     │
│     id=456: RRF = 1/(60+2) = 0.0161                                   │
│     id=789: RRF = 1/(60+2) = 0.0161                                   │
└─────────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────────┐
│                      Output                                           │
│   ────────                                                            │
│   {                                                                   │
│     id: 123, fused_score: 0.0325, vector_dist: 0.15, bm25_score: 8.3 │
│     id: 789, fused_score: 0.0161, vector_dist: 0.22, bm25_score: 0   │
│     id: 456, fused_score: 0.0161, vector_dist: 0.35, bm25_score: 12.5│
│   }                                                                   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 5. 接口定义

### 5.1 C SDK 头文件

`engineering/include/db/vdb_query.h`

```c
/**
 * @file vdb_query.h
 * @brief MiniVecDB 多模查询接口
 */
#pragma once

#include "db/vdb_api.h"
#include "db/vector_types.h"
#include "db/core/vector_query.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ========== 查询请求/响应结构 ==========

/**
 * @brief 查询模式
 */
typedef enum VdbQueryMode_e {
    VDB_QUERY_MODE_VECTOR = 0,       /**< 仅向量检索 */
    VDB_QUERY_MODE_FILTERED = 1,     /**< 向量检索 + 标量过滤 */
    VDB_QUERY_MODE_HYBRID = 2,       /**< 混合检索（向量 + 全文）*/
    VDB_QUERY_MODE_MULTIMODAL = 3    /**< 多模态检索 */
} VdbQueryMode;

/**
 * @brief 融合策略
 */
typedef enum VdbFusionStrategy_e {
    VDB_FUSION_RRF = 0,              /**< Reciprocal Rank Fusion */
    VDB_FUSION_WEIGHTED = 1,         /**< 加权求和 */
    VDB_FUSION_RERANK = 2            /**< 重排序融合 */
} VdbFusionStrategy;

/**
 * @brief 查询请求
 */
typedef struct VdbQueryRequest_s {
    VdbQueryMode mode;               /**< 查询模式 */
    const char *collection;          /**< 集合名称 */

    /* 向量查询字段 */
    const float *vector;             /**< 查询向量 */
    int32_t dims;                    /**< 向量维度 */
    int32_t top_k;                   /**< 返回结果数 */

    /* 标量过滤字段 */
    const char *filter;              /**< 过滤表达式 */

    /* 全文检索字段 */
    const char *text;                /**< 查询文本 */
    float bm25_weight;               /**< BM25 权重 */

    /* 混合控制字段 */
    float vector_weight;             /**< 向量权重 */
    VdbFusionStrategy fusion;        /**< 融合策略 */
    bool enable_rerank;              /**< 启用重排序 */
    bool enable_mmr;                 /**< 启用 MMR */
    float mmr_lambda;                /**< MMR lambda */

    /* 分页字段 */
    int32_t offset;                  /**< 偏移量 */
    int32_t limit;                   /**< 限制数量 */

    /* 执行配置 */
    int32_t ef_search;               /**< HNSW ef 参数 */
    int32_t timeout_ms;              /**< 超时时间 */
    bool with_metadata;              /**< 返回元数据 */
    bool with_scores;                /**< 返回分数 */
    bool profiling;                  /**< 启用剖析 */
} VdbQueryRequest;

/**
 * @brief 单条查询结果
 */
typedef struct VdbQueryItem_s {
    int64_t id;                      /**< 向量 ID */
    float distance;                  /**< 距离 */
    float score;                     /**< 分数 */
    char *metadata;                  /**< 元数据 JSON */
} VdbQueryItem;

/**
 * @brief 查询统计
 */
typedef struct VdbQueryStats_s {
    int64_t parse_time_us;           /**< 解析耗时 */
    int64_t plan_time_us;            /**< 计划耗时 */
    int64_t execute_time_us;         /**< 执行耗时 */
    int64_t total_time_us;           /**< 总耗时 */
    int32_t candidates;              /**< 候选数 */
    char index_used[64];             /**< 使用的索引 */
} VdbQueryStats;

/**
 * @brief 查询响应
 */
typedef struct VdbQueryResponse_s {
    VdbQueryItem *items;             /**< 结果数组 */
    int32_t count;                   /**< 结果数量 */
    int32_t total;                   /**< 总数 */
    VdbQueryStats stats;             /**< 统计信息 */
    int32_t error_code;              /**< 错误码 */
    char error_msg[256];             /**< 错误信息 */
} VdbQueryResponse;

// ========== 过滤表达式 API ==========

/**
 * @brief 解析过滤表达式
 * @param expr 表达式字符串
 * @param error 错误信息（可为 NULL）
 * @return AST 节点，失败返回 NULL
 */
void *vdb_filter_parse(const char *expr, char *error);

/**
 * @brief 释放过滤表达式 AST
 */
void vdb_filter_free(void *ast);

/**
 * @brief 获取 AST 复杂度估计
 */
int32_t vdb_filter_complexity(const void *ast);

// ========== 查询 API ==========

/**
 * @brief 向量查询（基础）
 * @param db 数据库句柄
 * @param collection 集合名称
 * @param vector 查询向量
 * @param k 返回结果数
 * @param result 输出结果
 * @return 0 成功，非 0 失败
 */
int vdb_vector_query(vdb_handle_t *db,
                     const char *collection,
                     const float *vector,
                     int32_t k,
                     vdb_query_result_t **result);

/**
 * @brief 带过滤的向量查询
 * @param db 数据库句柄
 * @param collection 集合名称
 * @param vector 查询向量
 * @param k 返回结果数
 * @param filter 过滤表达式（类 SQL 字符串）
 * @param result 输出结果
 * @return 0 成功，非 0 失败
 */
int vdb_vector_query_filtered(vdb_handle_t *db,
                               const char *collection,
                               const float *vector,
                               int32_t k,
                               const char *filter,
                               vdb_query_result_t **result);

/**
 * @brief 混合查询（向量 + 全文）
 * @param db 数据库句柄
 * @param collection 集合名称
 * @param vector 查询向量（可为 NULL）
 * @param text 查询文本（可为 NULL）
 * @param k 返回结果数
 * @param fusion 融合策略
 * @param vector_weight 向量权重
 * @param text_weight 文本权重
 * @param result 输出结果
 * @return 0 成功，非 0 失败
 */
int vdb_hybrid_query(vdb_handle_t *db,
                      const char *collection,
                      const float *vector,
                      const char *text,
                      int32_t k,
                      VdbFusionStrategy fusion,
                      float vector_weight,
                      float text_weight,
                      vdb_query_result_t **result);

/**
 * @brief 多模态查询（完整功能）
 * @param db 数据库句柄
 * @param request 查询请求
 * @param response 输出响应
 * @return 0 成功，非 0 失败
 */
int vdb_multimodal_query(vdb_handle_t *db,
                          const VdbQueryRequest *request,
                          VdbQueryResponse **response);

/**
 * @brief 释放查询结果
 */
void vdb_query_result_free(vdb_query_result_t *result);

/**
 * @brief 释放查询响应
 */
void vdb_query_response_free(VdbQueryResponse *response);

// ========== 查询统计 API ==========

/**
 * @brief 获取查询统计
 */
int vdb_query_get_stats(vdb_handle_t *db,
                         const char *collection,
                         VdbQueryStats *stats);

/**
 * @brief 重置查询统计
 */
int vdb_query_reset_stats(vdb_handle_t *db, const char *collection);

#ifdef __cplusplus
}
#endif
```

### 5.2 REST API 端点规范

```
POST /api/v1/collections/{collection}/query
Content-Type: application/json

Request Body:
{
  "query": {
    "vector": [0.1, 0.2, ...],      // 向量查询输入
    "text": "手机 5G",              // 全文查询输入
    "k": 10                          // 返回结果数
  },
  "filter": "price < 5000 AND category = 'electronics'",
  "hybrid": {
    "vector_weight": 0.7,
    "bm25_weight": 0.3,
    "strategy": "rrf"               // "rrf" | "weighted"
  },
  "options": {
    "ef_search": 64,
    "enable_rerank": true,
    "enable_mmr": false,
    "mmr_lambda": 0.7,
    "with_metadata": true,
    "with_scores": true,
    "profiling": false
  },
  "pagination": {
    "offset": 0,
    "limit": 10
  }
}

Response:
{
  "items": [
    {
      "id": 123,
      "distance": 0.15,
      "score": 0.92,
      "metadata": {"price": 3999, "category": "electronics"}
    },
    ...
  ],
  "count": 10,
  "total": 156,
  "stats": {
    "parse_time_us": 120,
    "plan_time_us": 80,
    "execute_time_us": 15230,
    "total_time_us": 15430,
    "candidates": 100,
    "index_used": "hnsw"
  },
  "error": null
}
```

### 5.3 CLI 命令规范

```bash
# 向量查询
vdb query vector -c products -k 10 --vector 0.1,0.2,0.3

# 带过滤的向量查询
vdb query filtered -c products -k 10 --vector 0.1,0.2,0.3 \
    --filter "price < 5000 AND category = 'electronics'"

# 混合查询
vdb query hybrid -c products -k 10 --vector 0.1,0.2,0.3 \
    --text "手机 5G" \
    --vector-weight 0.7 --bm25-weight 0.3 \
    --fusion rrf

# 多模态查询
vdb query multimodal -c products -k 10 \
    --vector 0.1,0.2,0.3 \
    --text "手机 5G" \
    --filter "price < 5000" \
    --vector-weight 0.6 --bm25-weight 0.4 \
    --enable-rerank --enable-mmr --mmr-lambda 0.7 \
    --with-metadata --profiling \
    --offset 0 --limit 10

# 查询统计
vdb query stats -c products

# 重置统计
vdb query stats -c products --reset
```

---

## 6. 与 Wave 2 实现规划对齐

### 6.1 实现依赖关系

```
Wave 1-④ 多模查询设计
       │
       ├──→ Wave 2-⑤ 统一 API 层
       │         ├── C SDK 实现 (vdb_query.h)
       │         ├── REST API 实现
       │         └── CLI 实现
       │
       ├──→ Wave 2-⑥ 过滤表达式引擎
       │         ├── 词法/语法分析器
       │         ├── AST 结构与操作
       │         └── 过滤执行器
       │
       └──→ Wave 2-⑦ 多模查询实现
                 ├── 混合检索执行器
                 ├── RRF/加权融合
                 ├── 重排序集成
                 └── MMR 多样性
```

### 6.2 关键设计决策

1. **过滤语法**：采用类 SQL WHERE 子句风格，平衡用户友好度和解析复杂度
2. **融合策略**：默认 RRF，支持加权求和，可扩展重排序
3. **接口层次**：C SDK → REST API → CLI 三层分离，便于多语言 SDK 扩展
4. **查询模型**：与 Wave 1-③ 的执行器架构对齐，复用 VectorScan/Filter/Limit 算子

### 6.3 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| 过滤表达式解析复杂度高 | 开发周期长 | 考虑使用 ANTLR/RE2C 生成 lexer/parser |
| RRF 参数 k 需要调优 | 用户体验差 | 提供合理默认值（60），文档说明调优方法 |
| 混合检索性能开销 | 查询延迟增加 | 支持并行执行向量和 BM25 检索 |
| 元数据过滤下推 | 索引层支持有限 | 设计 filter_pushdown 接口，分层执行 |

---

## 7. 参考文献

- [Milvus Boolean Expression](https://milvus.io/docs/boolean.md)
- [Qdrant Filtering](https://qdrant.tech/documentation/concepts/filtering/)
- [Pinecone Metadata Filtering](https://docs.pinecone.io/guides/data/filter-with-metadata)
- [Weaviate GraphQL Filters](https://weaviate.io/developers/weaviate/search/filters)
- [Reciprocal Rank Fusion](https://plg.uwaterloo.ca/~gvcormac/cormacksigir09-rrf.pdf)
- [BM25 Algorithm](https://en.wikipedia.org/wiki/Okapi_BM25)
