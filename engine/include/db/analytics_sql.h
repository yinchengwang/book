/**
 * @file analytics_sql.h
 * @brief Analytics SQL 方言支持头文件
 *
 * 提供 DuckDB/ClickHouse 风格的分析函数支持，包括：
 * - 窗口函数（移动平均、排名、Lead/Lag 等）
 * - 表采样（Bernoulli、Reservoir）
 * - 近似计算（HyperLogLog、T-Digest）
 */
#ifndef DB_ANALYTICS_SQL_H
#define DB_ANALYTICS_SQL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 分析函数类型枚举
 * ======================================================================== */

/**
 * @brief 分析函数类型
 */
typedef enum {
    ANALYTIC_FUNC_MOVING_AVG = 0,   /**< 移动平均 */
    ANALYTIC_FUNC_MOVING_SUM,        /**< 移动求和 */
    ANALYTIC_FUNC_LEAD,              /**< 前导函数 */
    ANALYTIC_FUNC_LAG,               /**< 滞后函数 */
    ANALYTIC_FUNC_FIRST_VALUE,       /**< 窗口内第一个值 */
    ANALYTIC_FUNC_LAST_VALUE,        /**< 窗口内最后一个值 */
    ANALYTIC_FUNC_RANK,              /**< 排名（并列跳号） */
    ANALYTIC_FUNC_DENSE_RANK,        /**< 密集排名（并列不跳号） */
    ANALYTIC_FUNC_PERCENT_RANK,      /**< 百分比排名 */
    ANALYTIC_FUNC_NTILE,             /**< 分桶函数 */
} analytic_func_type_t;

/* ========================================================================
 * 窗口帧类型
 * ======================================================================== */

/**
 * @brief 窗口帧边界类型
 */
typedef enum {
    FRAME_UNBOUNDED_PRECEDING = -2,  /**< 无界前驱 */
    FRAME_CURRENT_ROW = 0,           /**< 当前行 */
    FRAME_UNBOUNDED_FOLLOWING = 2,   /**< 无界后继 */
} analytic_frame_boundary_t;

/* ========================================================================
 * 采样方法
 * ======================================================================== */

/**
 * @brief 表采样方法
 */
typedef enum {
    SAMPLE_BERNOULLI = 0,   /**< Bernoulli 采样（行级别概率） */
    SAMPLE_RESERVOIR,       /**< Reservoir 采样（固定大小） */
} sample_method_t;

/* ========================================================================
 * 数据结构定义
 * ======================================================================== */

/**
 * @brief 窗口函数定义
 */
typedef struct analytic_window_s {
    analytic_func_type_t func_type;           /**< 函数类型 */
    char *partition_by[16];                    /**< 分区列名数组 */
    int partition_count;                       /**< 分区列数量 */
    char *order_by[16];                        /**< 排序列名数组 */
    int order_count;                           /**< 排序列数量 */
    int frame_start;                           /**< 帧起始偏移（负数表示 PRECEDING） */
    int frame_end;                             /**< 帧结束偏移（正数表示 FOLLOWING） */
    int frame_rows;                            /**< 行窗口大小（用于移动窗口） */
} analytic_window_t;

/**
 * @brief 采样配置
 */
typedef struct sample_config_s {
    sample_method_t method;       /**< 采样方法 */
    float ratio;                  /**< 采样比例 (0.0, 1.0] */
    uint32_t seed;                /**< 随机种子 */
    bool deterministic;           /**< 是否确定性采样（可复现） */
    uint32_t reservoir_size;      /**< Reservoir 采样大小 */
} sample_config_t;

/* ========================================================================
 * HyperLogLog 近似计数结构
 * ======================================================================== */

/**
 * @brief HyperLogLog sketch 结构
 *
 * 使用 64 个桶的 HyperLogLog 实现，误差约 1.6%
 */
typedef struct hll_sketch_s {
    uint8_t *buckets;             /**< 桶数组 */
    uint32_t num_buckets;         /**< 桶数量（必须是 2 的幂） */
    uint8_t precision;            /**< 精度参数 p (num_buckets = 2^p) */
    double alpha;                 /**< 修正系数 */
} hll_sketch_t;

/* ========================================================================
 * T-Digest 近似分位数结构
 * ======================================================================== */

/**
 * @brief T-Digest 单个簇
 */
typedef struct tdigest_cluster_s {
    double mean;                  /**< 簇均值 */
    double count;                 /**< 簇计数 */
    double weight;                /**< 簇权重 */
} tdigest_cluster_t;

/**
 * @brief T-Digest sketch 结构
 *
 * 用于近似中位数、分位数等统计量
 */
typedef struct tdigest_s {
    tdigest_cluster_t *clusters;  /**< 簇数组 */
    uint32_t num_clusters;        /**< 当前簇数量 */
    uint32_t max_clusters;        /**< 最大簇数量 */
    double total_count;           /**< 总计数 */
    double compression;           /**< 压缩参数（控制精度） */
} tdigest_t;

/* ========================================================================
 * API 函数声明
 * ======================================================================== */

/**
 * @brief 初始化窗口函数结构
 * @param window 窗口函数结构指针
 * @param func_type 函数类型
 */
void analytic_window_init(analytic_window_t *window, analytic_func_type_t func_type);

/**
 * @brief 释放窗口函数结构中的动态内存
 * @param window 窗口函数结构指针
 */
void analytic_window_destroy(analytic_window_t *window);

/**
 * @brief 添加分区列
 * @param window 窗口函数结构指针
 * @param column_name 列名（会复制）
 * @return 成功返回 0，失败返回 -1
 */
int analytic_window_add_partition(analytic_window_t *window, const char *column_name);

/**
 * @brief 添加排序列
 * @param window 窗口函数结构指针
 * @param column_name 列名（会复制）
 * @return 成功返回 0，失败返回 -1
 */
int analytic_window_add_order(analytic_window_t *window, const char *column_name);

/**
 * @brief 计算窗口函数
 *
 * @param func_name 函数名称（用于日志和调试）
 * @param values 输入值数组
 * @param count 输入值数量
 * @param window 窗口函数配置
 * @param results 输出结果数组（必须预分配与 values 相同大小）
 * @return 成功返回 0，失败返回 -1
 */
int analytic_evaluate_window(
    const char *func_name,
    const float *values,
    uint32_t count,
    const analytic_window_t *window,
    float *results
);

/**
 * @brief 表采样（通用接口）
 *
 * @param table 表数据指针
 * @param config 采样配置
 * @param result 采样结果
 * @return 成功返回采样的行数，失败返回 -1
 */
int analytic_table_sample(
    const void *table,
    const sample_config_t *config,
    void *result
);

/**
 * @brief Bernoulli 采样
 *
 * 对输入数组进行 Bernoulli 采样
 *
 * @param values 输入值数组
 * @param count 输入值数量
 * @param ratio 采样比例 (0.0, 1.0]
 * @param seed 随机种子（0 表示使用当前时间）
 * @param output 输出缓冲区
 * @param output_size 输出缓冲区大小
 * @return 实际输出的元素数量
 */
int analytic_bernoulli_sample(
    const float *values,
    uint32_t count,
    float ratio,
    uint32_t seed,
    float *output,
    uint32_t output_size
);

/**
 * @brief Reservoir 采样
 *
 * 从输入数组中进行 Reservoir 采样，保证每个元素被选中的概率相等
 *
 * @param values 输入值数组
 * @param count 输入值数量
 * @param reservoir_size 采样大小
 * @param seed 随机种子（0 表示使用当前时间）
 * @param output 输出缓冲区（必须至少 reservoir_size 大小）
 * @return 实际输出的元素数量
 */
int analytic_reservoir_sample(
    const float *values,
    uint32_t count,
    uint32_t reservoir_size,
    uint32_t seed,
    float *output
);

/* ========================================================================
 * 近似计算 API
 * ======================================================================== */

/**
 * @brief 近似计数不同值（HyperLogLog）
 *
 * @param column 列数据指针
 * @param count 数据数量
 * @return 估计的不同值数量
 */
float analytic_approx_count_distinct(const void *column, uint32_t count);

/**
 * @brief 近似中位数（T-Digest）
 *
 * @param values 输入值数组
 * @param count 输入值数量
 * @return 估计的中位数
 */
float analytic_approx_median(const float *values, uint32_t count);

/**
 * @brief 近似分位数（T-Digest）
 *
 * @param values 输入值数组
 * @param count 输入值数量
 * @param percentile 百分位 (0.0, 1.0]
 * @return 估计的分位数值
 */
float analytic_approx_percentile(const float *values, uint32_t count, float percentile);

/* ========================================================================
 * HyperLogLog API
 * ======================================================================== */

/**
 * @brief 创建 HyperLogLog sketch
 * @param precision 精度参数 p（桶数 = 2^p，推荐 6-14）
 * @return HLL sketch 指针，失败返回 NULL
 */
hll_sketch_t *hll_create(uint8_t precision);

/**
 * @brief 销毁 HyperLogLog sketch
 * @param sketch sketch 指针
 */
void hll_destroy(hll_sketch_t *sketch);

/**
 * @brief 向 HyperLogLog 插入元素
 * @param sketch sketch 指针
 * @param hash_value 元素的哈希值
 */
void hll_add(hll_sketch_t *sketch, uint64_t hash_value);

/**
 * @brief 获取 HyperLogLog 的估计基数
 * @param sketch sketch 指针
 * @return 估计的不同元素数量
 */
double hll_estimate(const hll_sketch_t *sketch);

/**
 * @brief 合并两个 HyperLogLog sketch
 * @param dest 目标 sketch
 * @param src 源 sketch
 * @return 成功返回 0，失败返回 -1
 */
int hll_merge(hll_sketch_t *dest, const hll_sketch_t *src);

/* ========================================================================
 * T-Digest API
 * ======================================================================== */

/**
 * @brief 创建 T-Digest sketch
 * @param compression 压缩参数（推荐 100-1000，越大越精确）
 * @return T-Digest 指针，失败返回 NULL
 */
tdigest_t *tdigest_create(double compression);

/**
 * @brief 销毁 T-Digest sketch
 * @param digest T-Digest 指针
 */
void tdigest_destroy(tdigest_t *digest);

/**
 * @brief 向 T-Digest 添加值
 * @param digest T-Digest 指针
 * @param value 要添加的值
 */
void tdigest_add(tdigest_t *digest, double value);

/**
 * @brief 获取 T-Digest 的中位数
 * @param digest T-Digest 指针
 * @return 中位数
 */
double tdigest_median(const tdigest_t *digest);

/**
 * @brief 获取 T-Digest 的指定分位数
 * @param digest T-Digest 指针
 * @param percentile 百分位 (0.0, 1.0]
 * @return 分位数值
 */
double tdigest_quantile(const tdigest_t *digest, double percentile);

/**
 * @brief 合并两个 T-Digest
 * @param dest 目标 T-Digest
 * @param src 源 T-Digest
 * @return 成功返回 0，失败返回 -1
 */
int tdigest_merge(tdigest_t *dest, const tdigest_t *src);

#ifdef __cplusplus
}
#endif

#endif /* DB_ANALYTICS_SQL_H */
