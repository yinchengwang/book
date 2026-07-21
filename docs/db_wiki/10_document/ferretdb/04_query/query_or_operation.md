# FerretDB 查询或操作引擎

## 学习目标

- 理解 FerretDB 的查询/操作执行流程
- 掌握 MongoDB 查询到 SQL 的翻译机制
- 了解聚合管道的实现方式
- 对比 FerretDB 与项目 algo/ 模块的关联

## 查询执行流程

FerretDB 的核心职责是将 MongoDB 风格的查询和操作翻译为 PostgreSQL 的 SQL 语句执行。整个执行流程分为协议解析、命令路由、查询翻译、SQL 执行和结果转换五个阶段。

```mermaid
graph TB
    subgraph "1. 协议接收层"
        CLIENT["MongoDB 客户端"]
        WIRE["Wire Protocol Handler<br/>OpMsg/OpQuery 解析"]
        BSON_PARSE["BSON 文档解析"]
    end

    subgraph "2. 命令路由层"
        CMD_ROUTER["命令路由器"]
        CRUD["CRUD 处理器<br/>find/insert/update/delete"]
        AGG["聚合处理器<br/>aggregate"]
        ADMIN["管理处理器<br/>create/drop/listCollections"]
        INDEX["索引处理器<br/>createIndexes/dropIndexes"]
    end

    subgraph "3. 查询翻译层"
        OP_MAP["操作符映射器<br/>MongoDB 操作符 → SQL"]
        PROJ_TRANS["投影翻译器<br/>projection → SELECT"]
        FILTER_TRANS["过滤翻译器<br/>filter → WHERE"]
        SORT_TRANS["排序翻译器<br/>sort → ORDER BY"]
        PIPELINE_TRANS["管道翻译器<br/>聚合阶段 → SQL 子句"]
    end

    subgraph "4. SQL 执行层"
        SQL_BUILDER["SQL 构建器"]
        PG_DRV["PostgreSQL Driver<br/>pgx"]
        PG_EXEC["PostgreSQL 执行"]
    end

    subgraph "5. 结果转换层"
        JSONB_BSON["JSONB → BSON"]
        CURSOR["Cursor 构建"]
        RESPONSE["OpMsg 响应"]
    end

    CLIENT --> WIRE --> BSON_PARSE --> CMD_ROUTER
    CMD_ROUTER --> CRUD
    CMD_ROUTER --> AGG
    CMD_ROUTER --> ADMIN
    CMD_ROUTER --> INDEX
    CRUD --> OP_MAP --> FILTER_TRANS
    CRUD --> PROJ_TRANS
    CRUD --> SORT_TRANS
    AGG --> PIPELINE_TRANS
    FILTER_TRANS --> SQL_BUILDER
    PROJ_TRANS --> SQL_BUILDER
    SORT_TRANS --> SQL_BUILDER
    PIPELINE_TRANS --> SQL_BUILDER
    SQL_BUILDER --> PG_DRV --> PG_EXEC
    PG_EXEC --> JSONB_BSON --> CURSOR --> RESPONSE
```

### 执行流程示例

以 `find` 命令为例，展示完整的执行流程：

```mermaid
sequenceDiagram
    participant C as 客户端
    participant W as Wire Protocol
    participant R as 命令路由
    participant T as 查询翻译
    participant P as PostgreSQL
    participant X as 结果转换

    C->>W: OpMsg {find: "users", filter: {age: {$gt: 18}}}
    W->>W: 解析 BSON 文档
    W->>R: 路由到 find 命令
    R->>T: 传递查询参数
    T->>T: $gt: 18 → WHERE (doc->>'age')::int > 18
    T->>P: SELECT _ferretdb_document FROM users WHERE ...
    P->>P: 执行 SQL 查询
    P-->>T: 返回 JSONB 行
    T->>X: 传递结果集
    X->>X: JSONB → BSON 转换
    X->>X: 构建 cursor
    X-->>C: 返回 OpMsg 响应
```

## 核心算法和数据结构

### 查询操作符映射

FerretDB 将 MongoDB 查询操作符映射为 SQL 条件表达式：

```mermaid
graph LR
    subgraph "比较操作符"
        EQ["$eq → ="]
        NE["$ne → <>"]
        GT["$gt → >"]
        GTE["$gte → >="]
        LT["$lt → <"]
        LTE["$lte → <="]
    end

    subgraph "逻辑操作符"
        AND["$and → AND"]
        OR["$or → OR"]
        NOT["$not → NOT"]
    end

    subgraph "数组操作符"
        IN["$in → IN"]
        NIN["$nin → NOT IN"]
        ALL["$all → 逐元素检查"]
        ELEM["$elemMatch → EXISTS 子查询"]
    end

    subgraph "存在性操作符"
        EXISTS["$exists → ? 操作符"]
        TYPE["$type → JSONB 类型检查"]
    end

    subgraph "正则操作符"
        REGEX["$regex → ~ 正则匹配"]
    end
```

### 操作符映射表

| MongoDB 操作符 | SQL 翻译 | 示例 |
|----------------|----------|------|
| `$eq` | `=` | `{name: {$eq: "foo"}}` → `WHERE doc->>'name' = 'foo'` |
| `$ne` | `<>` | `{name: {$ne: "foo"}}` → `WHERE doc->>'name' <> 'foo'` |
| `$gt` | `>` | `{age: {$gt: 18}}` → `WHERE (doc->>'age')::int > 18` |
| `$gte` | `>=` | `{age: {$gte: 18}}` → `WHERE (doc->>'age')::int >= 18` |
| `$lt` | `<` | `{age: {$lt: 65}}` → `WHERE (doc->>'age')::int < 65` |
| `$lte` | `<=` | `{age: {$lte: 65}}` → `WHERE (doc->>'age')::int <= 65` |
| `$in` | `IN` | `{status: {$in: ["A","B"]}}` → `WHERE doc->>'status' IN ('A','B')` |
| `$nin` | `NOT IN` | `{status: {$nin: ["X"]}}` → `WHERE doc->>'status' NOT IN ('X')` |
| `$and` | `AND` | `{$and: [{a:1},{b:2}]}` → `WHERE ... AND ...` |
| `$or` | `OR` | `{$or: [{a:1},{b:2}]}` → `WHERE ... OR ...` |
| `$not` | `NOT` | `{age: {$not: {$gt: 18}}}` → `WHERE NOT ((doc->>'age')::int > 18)` |
| `$exists` | `?` | `{phone: {$exists: true}}` → `WHERE doc ? 'phone'` |
| `$regex` | `~` | `{name: {$regex: "^foo"}}` → `WHERE doc->>'name' ~ '^foo'` |
| `$type` | `jsonb_typeof` | `{x: {$type: "string"}}` → `WHERE jsonb_typeof(doc->'x') = 'string'` |

### 查询翻译算法

```mermaid
graph TB
    subgraph "输入：MongoDB 查询"
        QUERY["{<br/>  find: 'users',<br/>  filter: {age: {$gt: 18}, status: 'active'},<br/>  sort: {name: 1},<br/>  limit: 10<br/>}"]
    end

    subgraph "翻译过程"
        PARSE["解析查询参数"]
        
        subgraph "filter 翻译"
            F1["age: {$gt: 18}<br/>→ (doc->>'age')::int > 18"]
            F2["status: 'active'<br/>→ doc->>'status' = 'active'"]
            F_AND["AND 连接"]
        end
        
        subgraph "sort 翻译"
            S1["name: 1<br/>→ ORDER BY doc->>'name' ASC"]
        end
        
        subgraph "limit 翻译"
            L1["limit: 10<br/>→ LIMIT 10"]
        end
    end

    subgraph "输出：SQL 查询"
        SQL["SELECT _ferretdb_document<br/>FROM \"db\".\"users\"<br/>WHERE (doc->>'age')::int > 18<br/>  AND doc->>'status' = 'active'<br/>ORDER BY doc->>'name' ASC<br/>LIMIT 10"]
    end

    QUERY --> PARSE
    PARSE --> F1 --> F_AND
    PARSE --> F2 --> F_AND
    PARSE --> S1
    PARSE --> L1
    F_AND --> SQL
    S1 --> SQL
    L1 --> SQL
```

### 投影翻译

```mermaid
graph LR
    subgraph "MongoDB 投影"
        P1["{name: 1, age: 1, _id: 0}"]
    end

    subgraph "翻译过程"
        TRANS["投影字段提取"]
        SEL1["name: 1 → doc->'name'"]
        SEL2["age: 1 → doc->'age'"]
        EXC1["_id: 0 → 排除 _id"]
    end

    subgraph "SQL SELECT"
        SQL["SELECT jsonb_build_object(<br/>  'name', doc->'name',<br/>  'age', doc->'age'<br/>) AS result"]
    end

    P1 --> TRANS --> SEL1 --> SQL
    TRANS --> SEL2 --> SQL
    TRANS --> EXC1
```

### 聚合管道翻译

FerretDB 支持将 MongoDB 聚合管道翻译为 SQL：

```mermaid
graph TB
    subgraph "MongoDB 聚合管道"
        A1["$match: {status: 'active'}"]
        A2["$group: {_id: '$city', total: {$sum: '$amount'}}"]
        A3["$sort: {total: -1}"]
        A4["$limit: 10"]
    end

    subgraph "SQL 子句映射"
        S1["WHERE doc->>'status' = 'active'"]
        S2["GROUP BY doc->>'city'<br/>SELECT SUM((doc->>'amount')::numeric)"]
        S3["ORDER BY total DESC"]
        S4["LIMIT 10"]
    end

    subgraph "完整 SQL"
        SQL["SELECT doc->>'city' AS _id,<br/>       SUM((doc->>'amount')::numeric) AS total<br/>FROM collection<br/>WHERE doc->>'status' = 'active'<br/>GROUP BY doc->>'city'<br/>ORDER BY total DESC<br/>LIMIT 10"]
    end

    A1 --> S1 --> SQL
    A2 --> S2 --> SQL
    A3 --> S3 --> SQL
    A4 --> S4 --> SQL
```

### 聚合阶段映射表

| MongoDB 阶段 | SQL 映射 | 说明 |
|--------------|----------|------|
| `$match` | `WHERE` | 过滤条件 |
| `$group` | `GROUP BY` + 聚合函数 | 分组聚合 |
| `$sort` | `ORDER BY` | 排序 |
| `$limit` | `LIMIT` | 限制结果数 |
| `$skip` | `OFFSET` | 跳过结果数 |
| `$project` | `SELECT` | 投影选择 |
| `$unwind` | `jsonb_array_elements` | 数组展开 |
| `$lookup` | `LEFT JOIN` | 关联查询 |
| `$addFields` | `SELECT` + 计算字段 | 添加字段 |
| `$count` | `COUNT` | 计数 |

### 数据结构设计

FerretDB 内部使用以下核心数据结构处理查询：

```go
// 查询条件（Go 语言）
type Filter struct {
    Field    string      // 字段名
    Operator string      // 操作符
    Value    interface{} // 值
}

// 查询规格
type QuerySpec struct {
    Database   string    // 数据库名
    Collection string    // 集合名
    Filter     bson.D    // 过滤条件
    Projection bson.D    // 投影
    Sort       bson.D    // 排序
    Limit      int64     // 限制
    Skip       int64     // 跳过
}

// SQL 构建器
type SQLBuilder struct {
    SelectClause  string
    FromClause    string
    WhereClause   string
    OrderByClause string
    LimitClause   string
    OffsetClause  string
}
```

## 与项目 algo/ 模块的关联

### 模块对应关系

```mermaid
graph TB
    subgraph "FerretDB 核心算法"
        F_OP_MAP["操作符映射<br/>MongoDB 操作符 → SQL"]
        F_PIPE_TRANS["管道翻译<br/>聚合阶段 → SQL 子句"]
        F_TYPE_CONV["类型转换<br/>BSON ↔ JSONB"]
        F_FILTER_OPT["过滤优化<br/>条件简化"]
    end

    subgraph "项目 algo-prod 模块"
        P_SORT["sort.h<br/>排序算法"]
        P_DIST["distance.h<br/>距离计算"]
        P_QUANT["quantization<br/>PQ/SQ/RQ 量化"]
        P_SEARCH["二分查找<br/>binary_search"]
    end

    subgraph "项目 self_made 模块"
        S_HASH["uthash<br/>哈希表"]
        S_TRIE["trie.h<br/>字典树"]
    end

    subgraph "项目 redis 移植模块"
        R_SKIPLIST["redis_skiplist<br/>跳表"]
        R_SDS["redis_sds<br/>字符串"]
        R_LIST["redis_adlist<br/>链表"]
    end

    F_OP_MAP -.->|"映射算法"| S_HASH
    F_PIPE_TRANS -.->|"排序算法"| P_SORT
    F_TYPE_CONV -.->|"距离计算"| P_DIST
    F_FILTER_OPT -.->|"索引查找"| P_SEARCH
```

### 算法关联分析

| FerretDB 算法 | 项目模块 | 关联说明 |
|---------------|----------|----------|
| 操作符映射 | uthash（哈希表） | 操作符映射表可用哈希表实现 O(1) 查找 |
| 聚合管道排序 | sort.h（排序算法） | `$sort` 阶段使用排序算法，支持多种排序方式 |
| 向量相似度查询 | distance.h（距离计算） | 向量检索需要 L2/余弦/内积距离计算 |
| 向量压缩存储 | quantization（PQ/SQ/RQ） | 向量索引使用量化压缩减少内存占用 |
| 文本索引 | trie.h（字典树） | 全文索引可用字典树实现前缀匹配 |
| 有序索引 | redis_skiplist（跳表） | 有序集合索引可用跳表实现 O(log n) 查找 |

### 具体算法对比

#### 排序算法对比

```mermaid
graph TB
    subgraph "FerretDB 排序"
        F_SORT["$sort 阶段"]
        F_SQL["ORDER BY 子句"]
        F_PG_SORT["PostgreSQL 排序<br/>快排/归并"]
    end

    subgraph "项目 sort.h"
        P_QUICK["sort_quick<br/>快速排序 O(n log n)"]
        P_MERGE["sort_merge<br/>归并排序 O(n log n)"]
        P_HEAP["sort_heap<br/>堆排序 O(n log n)"]
        P_RADIX["sort_radix_int32<br/>基数排序 O(dn)"]
    end

    F_SORT --> F_SQL --> F_PG_SORT
    F_PG_SORT -.->|"算法对应"| P_QUICK
    F_PG_SORT -.->|"算法对应"| P_MERGE
```

#### 距离计算对比

```mermaid
graph TB
    subgraph "FerretDB 向量查询"
        F_VEC["向量相似度查询"]
        F_L2["L2 距离"]
        F_COSINE["余弦相似度"]
        F_IP["内积"]
    end

    subgraph "项目 distance.h"
        P_L2SQR["distance_l2sqr<br/>L2 平方距离"]
        P_COSINE["DISTANCE_METRIC_COSINE<br/>余弦距离"]
        P_IP["DISTANCE_METRIC_INNER_PRODUCT<br/>内积"]
        P_HAMMING["DISTANCE_METRIC_HAMMING<br/>汉明距离"]
    end

    F_L2 --> P_L2SQR
    F_COSINE --> P_COSINE
    F_IP --> P_IP
```

### 可复用的算法模块

```c
// 项目 sort.h 可用于查询结果排序
typedef int (*sort_compare_fn)(const void *lhs, const void *rhs);

// 支持多种排序算法，根据数据特征选择
int sort_dispatch(sort_algorithm_t algorithm,
                  void *base,
                  size_t count,
                  size_t element_size,
                  sort_compare_fn compare);

// 项目 distance.h 可用于向量相似度查询
typedef enum distance_metric {
    DISTANCE_METRIC_L2_SQUARED = 0,   // L2 平方距离
    DISTANCE_METRIC_COSINE = 1,        // 余弦距离
    DISTANCE_METRIC_INNER_PRODUCT = 2, // 内积
    DISTANCE_METRIC_HAMMING = 3,       // 汉明距离
} distance_metric_t;

float distance_compute(distance_metric_t metric,
                       const float *lhs,
                       const float *rhs,
                       int32_t dims);

// 项目 quantization 可用于向量压缩
int pq_encode(const quantizer_t *q, const float *vector, uint8_t *code);
float pq_adc_distance(const quantizer_t *q, const uint8_t *code, const float *table);
```

### 应用场景映射

```mermaid
graph LR
    subgraph "FerretDB 查询场景"
        Q1["文本搜索"]
        Q2["向量检索"]
        Q3["范围查询"]
        Q4["聚合计算"]
    end

    subgraph "项目算法模块"
        A1["trie.h<br/>前缀匹配"]
        A2["distance.h + pq.h<br/>向量相似度"]
        A3["binary_search<br/>二分查找"]
        A4["sort.h<br/>排序聚合"]
    end

    Q1 --> A1
    Q2 --> A2
    Q3 --> A3
    Q4 --> A4
```

## 要点总结

- **执行流程**：FerretDB 查询分为协议解析 → 命令路由 → 查询翻译 → SQL 执行 → 结果转换五个阶段
- **操作符映射**：MongoDB 操作符系统映射为 SQL 条件表达式，支持比较、逻辑、数组、存在性等操作符
- **聚合管道**：聚合阶段逐个翻译为 SQL 子句，`$match` → `WHERE`，`$group` → `GROUP BY`，`$sort` → `ORDER BY`
- **算法关联**：项目的 sort、distance、quantization、trie 等模块可在查询引擎中复用

## 思考题

1. FerretDB 将 MongoDB 的 `$or` 操作符翻译为 SQL 的 `OR` 子句。如果 `$or` 包含大量条件，这种翻译方式的性能如何？PostgreSQL 是否有优化机制？

2. 项目 `sort.h` 提供了 10 种排序算法。如果为项目的查询引擎实现排序功能，应该如何选择合适的排序算法？考虑数据规模、内存限制、稳定性等因素。

3. 聚合管道中的 `$unwind` 阶段使用 PostgreSQL 的 `jsonb_array_elements` 函数实现。对于大型数组，这种方式的性能瓶颈在哪里？如何优化？
