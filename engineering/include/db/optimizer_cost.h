/**
 * @file optimizer_cost.h
 * @brief 查询优化器代价模型
 *
 * ## 代价模型概述
 *
 * 代价模型是查询优化的核心，用于估算不同执行计划的代价，
 * 选择代价最低的执行计划。
 *
 * ## 代价因素
 *
 * - I/O 代价：从磁盘读取数据的代价
 * - CPU 代价：处理数据的代价
 * - 内存代价：使用内存的代价
 * - 网络代价：分布式场景下的网络传输代价
 */

#ifndef DB_OPTIMIZER_COST_H
#define DB_OPTIMIZER_COST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 代价常量
 * ============================================================ */

/** 顺序读一页的代价（基准） */
#define COST_PAGE_SEQ_SCAN 1.0

/** 随机读一页的代价 */
#define COST_PAGE_RANDOM_SCAN 4.0

/** 处理一行的 CPU 代价 */
#define COST_CPU_ROW 0.01

/** 处理一个表达式的 CPU 代价 */
#define COST_CPU_EXPR 0.0025

/** 创建一个元组的代价 */
#define COST_TUPLE_CREATE 1.0

/** 一次比较操作的代价 */
#define COST_COMPARE 0.001

/** 哈希计算的代价 */
#define COST_HASH 0.005

/** 排序一行的代价 */
#define COST_SORT_ROW 0.005

/** 连接操作的代价因子 */
#define COST_JOIN_NESTED_LOOP 100.0
#define COST_JOIN_HASH 20.0
#define COST_JOIN_MERGE 15.0

/* ============================================================
 * 统计信息结构
 * ============================================================ */

/** 表的统计信息 */
typedef struct table_stats {
    uint32_t table_oid;       /**< 表OID */
    double n_rows;            /**< 行数估算 */
    double n_pages;           /**< 页面数 */
    double tuple_width;       /**< 元组平均宽度（字节） */
    double fillfactor;        /**< 填充因子 */
    double dead_tuple_ratio;  /**< 死亡元组比例 */
} table_stats_t;

/** 索引的统计信息 */
typedef struct index_stats {
    uint32_t index_oid;       /**< 索引OID */
    uint32_t table_oid;       /**< 所属表OID */
    double n_entries;         /**< 索引项数量 */
    double n_pages;           /**< 索引页面数 */
    double avg_key_width;     /**< 平均键宽度 */
    double n_distinct;        /**< 不同值数量 */
    double selectivity;       /**< 选择性（1/n_distinct） */
    double index_height;      /**< 索引高度 */
} index_stats_t;

/** 列的统计信息 */
typedef struct column_stats {
    uint32_t table_oid;       /**< 表OID */
    uint32_t column_id;       /**< 列ID */
    double n_distinct;        /**< 不同值数量 */
    double null_frac;         /**< NULL 值比例 */
    double avg_width;         /**< 平均宽度 */
    double correlation;       /**< 与物理顺序的相关性 */
} column_stats_t;

/* ============================================================
 * 代价估算结构
 * ============================================================ */

/** 代价估算结果 */
typedef struct cost_estimate {
    double total_cost;        /**< 总代价 */
    double startup_cost;      /**< 启动代价（开始前需要的工作） */
    double total_cost_ex;     /**< 总代价（展开） */

    /** 代价分解 */
    double disk_io_cost;      /**< 磁盘I/O代价 */
    double cpu_cost;          /**< CPU代价 */
    double memory_cost;       /**< 内存代价 */
    double network_cost;      /**< 网络代价（分布式） */

    /** 执行信息 */
    double rows_estimated;    /**< 估算行数 */
    double width_estimated;   /**< 估算宽度 */
} cost_estimate_t;

/** 扫描代价参数 */
typedef struct scan_cost_params {
    double page_fetch_cost;   /**< 读取一页的代价 */
    double tuple_fetch_cost;  /**< 读取一行的代价 */
    double cpu_tuple_cost;    /**< 处理一行的CPU代价 */
    double cpu_index_tuple_cost; /**< 处理索引行的CPU代价 */
    double cpu_operator_cost; /**< 操作符代价 */
    double page_priority;     /**< 页面优先级 */
} scan_cost_params_t;

/* ============================================================
 * 代价模型配置
 * ============================================================ */

/** 获取默认扫描代价参数 */
scan_cost_params_t cost_defaults_scan(void);

/** 获取默认连接代价参数 */
scan_cost_params_t cost_defaults_join(void);

/** 设置代价参数 */
void cost_set_params(const scan_cost_params_t *params);

/** 重置代价参数为默认值 */
void cost_reset_params(void);

/* ============================================================
 * 扫描代价估算
 * ============================================================ */

/**
 * @brief 估算顺序扫描代价
 * @param cost 输出：代价估算
 * @param pages 页面数
 * @param rows 行数
 * @param tuple_width 元组宽度
 */
void cost_seq_scan(cost_estimate_t *cost,
                   double pages,
                   double rows,
                   double tuple_width);

/**
 * @brief 估算索引扫描代价
 * @param cost 输出：代价估算
 * @param index_pages 索引页面数
 * @param heap_pages Heap 页面数
 * @param index_rows 索引返回行数
 * @param heap_rows Heap 返回行数
 * @param index_selectivity 索引选择性
 * @param index_depth 索引深度
 */
void cost_index_scan(cost_estimate_t *cost,
                     double index_pages,
                     double heap_pages,
                     double index_rows,
                     double heap_rows,
                     double index_selectivity,
                     double index_depth);

/**
 * @brief 估算位图扫描代价
 * @param cost 输出：代价估算
 * @param index_pages 索引页面数
 * @param heap_pages Heap 页面数
 * @param rows 返回行数
 * @param index_selectivity 索引选择性
 */
void cost_bitmap_scan(cost_estimate_t *cost,
                      double index_pages,
                      double heap_pages,
                      double rows,
                      double index_selectivity);

/**
 * @brief 估算索引创建代价
 * @param cost 输出：代价估算
 * @param table_rows 表行数
 * @param key_width 键宽度
 */
void cost_index_create(cost_estimate_t *cost,
                       double table_rows,
                       double key_width);

/* ============================================================
 * 连接代价估算
 * ============================================================ */

/**
 * @brief 估算嵌套循环连接代价
 * @param cost 输出：代价估算
 * @param outer_rows 外表行数
 * @param inner_rows 内表行数
 * @param inner_pages 内表页面数
 * @param join_cost_per_row 每行连接的代价
 */
void cost_nested_loop(cost_estimate_t *cost,
                      double outer_rows,
                      double inner_rows,
                      double inner_pages,
                      double join_cost_per_row);

/**
 * @brief 估算哈希连接代价
 * @param cost 输出：代价估算
 * @param outer_rows 外表行数
 * @param inner_rows 内表行数
 * @param hash_buckets 哈希桶数
 * @param build_pages 构建侧页面数
 * @param probe_pages 探测侧页面数
 */
void cost_hash_join(cost_estimate_t *cost,
                    double outer_rows,
                    double inner_rows,
                    double hash_buckets,
                    double build_pages,
                    double probe_pages);

/**
 * @brief 估算归并连接代价
 * @param cost 输出：代价估算
 * @param outer_rows 外表行数
 * @param inner_rows 内表行数
 * @param outer_pages 外表页面数
 * @param inner_pages 内表页面数
 * @param sorted 两侧是否已排序
 */
void cost_merge_join(cost_estimate_t *cost,
                     double outer_rows,
                     double inner_rows,
                     double outer_pages,
                     double inner_pages,
                     bool sorted);

/* ============================================================
 * 聚合代价估算
 * ============================================================ */

/**
 * @brief 估算哈希聚合代价
 * @param cost 输出：代价估算
 * @param rows 输入行数
 * @param groups 组数
 * @param hash_width 哈希宽度
 */
void cost_hash_agg(cost_estimate_t *cost,
                   double rows,
                   double groups,
                   double hash_width);

/**
 * @brief 估算排序聚合代价
 * @param cost 输出：代价估算
 * @param rows 输入行数
 * @param groups 组数
 * @param tuple_width 元组宽度
 */
void cost_sort_agg(cost_estimate_t *cost,
                   double rows,
                   double groups,
                   double tuple_width);

/* ============================================================
 * 排序代价估算
 * ============================================================ */

/**
 * @brief 估算排序代价
 * @param cost 输出：代价估算
 * @param rows 行数
 * @param tuple_width 元组宽度
 * @param work_mem 工作内存（字节）
 * @param enable_sort 启用排序优化
 */
void cost_sort(cost_estimate_t *cost,
               double rows,
               double tuple_width,
               size_t work_mem,
               bool enable_sort);

/* ============================================================
 * 工具函数
 * ============================================================ */

/**
 * @brief 比较两个代价估算
 * @param a 代价A
 * @param b 代价B
 * @return >0 如果 A > B，<0 如果 A < B，=0 如果相等
 */
int cost_compare(const cost_estimate_t *a, const cost_estimate_t *b);

/**
 * @brief 打印代价估算（调试用）
 * @param cost 代价估算
 * @param label 标签
 */
void cost_print(const cost_estimate_t *cost, const char *label);

#ifdef __cplusplus
}
#endif

#endif /* DB_OPTIMIZER_COST_H */
