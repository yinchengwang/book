/**
 * @file explain.h
 * @brief 查询执行计划分析工具公共接口
 *
 * 提供 EXPLAIN 命令的执行计划生成和格式化输出功能，
 * 参考 PostgreSQL 的 EXPLAIN 实现。
 */
#ifndef DB_TOOLS_EXPLAIN_H
#define DB_TOOLS_EXPLAIN_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * EXPLAIN 格式类型
 * ───────────────────────────────────────────────────────────────── */

/** EXPLAIN 输出格式 */
typedef enum ExplainFormat {
    EXPLAIN_FORMAT_TEXT,    /* 文本格式（默认） */
    EXPLAIN_FORMAT_JSON,    /* JSON 格式 */
    EXPLAIN_FORMAT_YAML     /* YAML 格式 */
} ExplainFormat;

/* ─────────────────────────────────────────────────────────────────
 * EXPLAIN 选项
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief EXPLAIN 命令选项
 */
typedef struct ExplainOptions {
    ExplainFormat format;      /* 输出格式 */
    bool          analyze;     /* 是否实际执行 */
    bool          costs;       /* 是否显示成本 */
    bool          buffers;     /* 是否显示缓冲区使用（需 analyze） */
    bool          timing;      /* 是否显示时间（需 analyze） */
    bool          verbose;     /* 是否显示详细信息 */
} ExplainOptions;

/* ─────────────────────────────────────────────────────────────────
 * 执行计划节点类型
 * ───────────────────────────────────────────────────────────────── */

/** 计划节点类型 */
typedef enum PlanNodeType {
    PLAN_NODE_SEQ_SCAN,        /* 顺序扫描 */
    PLAN_NODE_INDEX_SCAN,      /* 索引扫描 */
    PLAN_NODE_BITMAP_SCAN,     /* 位图扫描 */
    PLAN_NODE_NESTED_LOOP,     /* 嵌套循环连接 */
    PLAN_NODE_HASH_JOIN,       /* 哈希连接 */
    PLAN_NODE_MERGE_JOIN,      /* 归并连接 */
    PLAN_NODE_AGGREGATE,       /* 聚合 */
    PLAN_NODE_SORT,            /* 排序 */
    PLAN_NODE_LIMIT,           /* LIMIT */
    PLAN_NODE_GATHER           /* Gather（并行） */
} PlanNodeType;

/* ─────────────────────────────────────────────────────────────────
 * 执行计划节点
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 执行计划节点结构
 */
typedef struct ExplainPlanNode {
    PlanNodeType  type;              /* 节点类型 */
    const char   *relation_name;     /* 关系名（扫描节点） */
    const char   *index_name;        /* 索引名（索引扫描） */
    const char   *filter;            /* 过滤条件 */

    /* 成本信息 */
    double        startup_cost;      /* 启动成本 */
    double        total_cost;        /* 总成本 */
    int64_t       plan_rows;         /* 预估行数 */
    int           plan_width;        /* 行宽度 */

    /* 执行统计（analyze 时填充） */
    double        actual_time;       /* 实际执行时间（毫秒） */
    int64_t       actual_rows;       /* 实际返回行数 */
    int64_t       actual_loops;      /* 循环次数 */

    /* 缓冲区统计（buffers 时填充） */
    int64_t       shared_hit;        /* 共享缓冲区命中 */
    int64_t       shared_read;       /* 共享缓冲区读取 */
    int64_t       shared_dirtied;    /* 共享缓冲区脏页 */
    int64_t       shared_written;    /* 共享缓冲区写入 */

    /* 子计划 */
    struct ExplainPlanNode **children;  /* 子节点数组 */
    int                  n_children;    /* 子节点数量 */
} ExplainPlanNode;

/* ─────────────────────────────────────────────────────────────────
 * EXPLAIN 上下文
 * ───────────────────────────────────────────────────────────────── */

/** EXPLAIN 执行上下文 */
typedef struct ExplainContext_s ExplainContext;

/* ─────────────────────────────────────────────────────────────────
 * EXPLAIN API
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取默认 EXPLAIN 选项
 */
ExplainOptions explain_default_options(void);

/**
 * @brief 创建 EXPLAIN 上下文
 * @param options EXPLAIN 选项
 * @return 上下文句柄，NULL 表示失败
 */
ExplainContext *explain_create(ExplainOptions *options);

/**
 * @brief 销毁 EXPLAIN 上下文
 * @param ctx 上下文句柄
 */
void explain_destroy(ExplainContext *ctx);

/**
 * @brief 创建计划节点
 * @param type 节点类型
 * @return 计划节点，NULL 表示失败
 */
ExplainPlanNode *explain_create_node(PlanNodeType type);

/**
 * @brief 添加子计划节点
 * @param parent 父节点
 * @param child 子节点
 * @return 0 成功，非 0 失败
 */
int explain_add_child(ExplainPlanNode *parent, ExplainPlanNode *child);

/**
 * @brief 生成 EXPLAIN 输出
 * @param ctx 上下文
 * @param plan 计划根节点
 * @param fp 输出文件
 * @return 0 成功，非 0 失败
 */
int explain_output(ExplainContext *ctx, ExplainPlanNode *plan, FILE *fp);

/**
 * @brief 释放计划节点树
 * @param node 计划节点
 */
void explain_free_plan(ExplainPlanNode *node);

/**
 * @brief 输出 YAML 格式的执行计划
 * @param node 计划根节点
 * @param fp 输出文件流
 * @param indent 初始缩进层级
 * @return 0 成功，非 0 失败
 */
int explain_output_yaml(ExplainPlanNode *node, FILE *fp, int indent);

#ifdef __cplusplus
}
#endif

#endif /* DB_TOOLS_EXPLAIN_H */
