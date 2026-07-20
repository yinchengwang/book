# 与项目关联

## 学习目标

- 分析 Qdrant 设计对项目存储引擎的启发性
- 找出项目中可借鉴的关键技术点

## Segment 架构启发

Qdrant 的 Segment（Growing → Immutable → Merge）状态机设计，对项目中 Buffer Pool 的页面管理有直接参考意义：

```c
// 项目当前: 固定页面大小，统一管理
// 借鉴 Qdrant: 多级页面状态 + 异步合并

// 可设计的多级页面结构
typedef enum {
    PAGE_GROWING,    // 可写入（类似 Growing Segment）
    PAGE_SEALED,     // 只读（类似 Immutable Segment）
    PAGE_FROZEN      // 已压缩合并（类似 Merged）
} page_state_t;
```

## Payload 过滤设计

项目目前的 SQL 执行器支持简单的 WHERE 条件过滤，但不支持向量搜索中的"标量预过滤"：

```c
// 借鉴 Qdrant 的过滤设计
typedef enum {
    FILTER_MATCH,       // 精确匹配
    FILTER_RANGE,       // 范围 gte/lte/gt/lt
    FILTER_GEO_RADIUS,  // 地理半径
    FILTER_IN,          // IN 列表
    FILTER_AND,         // AND 组合
    FILTER_OR           // OR 组合
} filter_type_t;

typedef struct Filter {
    filter_type_t type;
    char field_name[64];
    union {
        MatchValue match;
        Range range;
        GeoCoord geo;
    };
    struct Filter *children;  // 组合条件
    int n_children;
} Filter;
```

## 向量索引的 HNSW 实现参考

Qdrant 自研 HNSW 的实现代码在 `lib/hnsw/` 目录，可以作为项目中向量引擎 HNSW 实现的参考。

## 要点总结

- Segment 状态机可优化项目 Buffer Pool 管理
- Payload 过滤设计可直接用于项目的标量过滤
- HNSW 自研实现的代码结构清晰
- 分组搜索算法可用于推荐类模块

## 思考题

1. 项目中如果实现类似 Segment 的多级页面管理，需要修改哪些模块？
2. Payload 过滤下推到存储引擎层，对 SQL 执行器有什么影响？
3. Qdrant 的 HNSW + Payload 协同过滤，如何应用到项目引擎？