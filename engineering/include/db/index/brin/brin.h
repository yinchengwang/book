/*
 * brin.h
 *
 * BRIN (Block Range Index) 块范围索引公共 API
 *
 * 核心原理：
 * - 将表按物理块划分为多个范围（Range）
 * - 每个范围维护一个摘要（Summary），包含该范围内所有键的 min/max
 * - 查询时，利用摘要快速跳过不相关的范围
 *
 * 特点：
 * - 高压缩比，适合超大规模数据
 * - 适合顺序访问模式（范围扫描）
 * - 内存占用极小
 */

#ifndef BRIN_H
#define BRIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* ── 类型定义 ── */

/* BRIN 索引句柄 */
typedef struct brin_index brin_index_t;

/* 键比较函数类型
 * @param a 键 a
 * @param b 键 b
 * @param ctx 用户上下文
 * @return < 0 if a < b, 0 if a == b, > 0 if a > b
 */
typedef int (*brin_compare_fn)(const void *a, const void *b, void *ctx);

/* ── 生命周期 API ── */

/**
 * @brief 创建 BRIN 索引
 * @param page_size 页面大小（字节）
 * @param pages_per_range 每个范围的页面数
 * @return 索引句柄，失败返回 NULL
 */
brin_index_t *brin_create(int page_size, int pages_per_range);

/**
 * @brief 销毁 BRIN 索引
 * @param idx 索引句柄
 */
void brin_destroy(brin_index_t *idx);

/* ── 配置 API ── */

/**
 * @brief 设置键比较函数
 * @param idx 索引句柄
 * @param compare 比较函数，NULL 使用默认的 memcmp
 * @param ctx 用户上下文，会传递给比较函数
 */
void brin_set_compare(brin_index_t *idx, brin_compare_fn compare, void *ctx);

/* ── 数据操作 API ── */

/**
 * @brief 插入单条记录
 * @param idx 索引句柄
 * @param page_num 页面号
 * @param key 键值
 * @param doc_id 文档 ID
 * @return 0 成功，-1 失败
 */
int brin_insert(brin_index_t *idx, int page_num, const void *key, int doc_id);

/**
 * @brief 批量插入记录
 * @param idx 索引句柄
 * @param page_nums 页面号数组
 * @param keys 键值数组
 * @param doc_ids 文档 ID 数组
 * @param count 插入数量
 * @return 成功插入的数量
 */
int brin_insert_batch(brin_index_t *idx, const int *page_nums,
                      const void **keys, const int *doc_ids, int count);

/* ── 查询 API ── */

/**
 * @brief 范围查询
 * @param idx 索引句柄
 * @param min_key 最小键
 * @param max_key 最大键
 * @param results 结果数组（输出，存储 doc_id）
 * @param count 结果数量（输入/输出）
 * @return 0 成功，-1 失败
 */
int brin_range_query(const brin_index_t *idx, const void *min_key, const void *max_key,
                     int *results, int *count);

/**
 * @brief 页面范围扫描
 * @param idx 索引句柄
 * @param start_page 起始页面
 * @param end_page 结束页面
 * @param results 结果数组（输出，存储 doc_id）
 * @param count 结果数量（输入/输出）
 * @return 0 成功，-1 失败
 */
int brin_scan(const brin_index_t *idx, int start_page, int end_page,
             int *results, int *count);

/* ── 维护 API ── */

/**
 * @brief 更新指定页面的摘要
 * @param idx 索引句柄
 * @param page_num 页面号
 * @return 0 成功，-1 失败
 */
int brin_update_summary(brin_index_t *idx, int page_num);

/**
 * @brief 获取索引统计信息
 * @param idx 索引句柄
 * @param out_n_ranges 输出范围数量（可为 NULL）
 * @param out_n_pages 输出页面数量（可为 NULL）
 * @param out_total_pages 输出总记录数（可为 NULL）
 */
void brin_stats(const brin_index_t *idx, int *out_n_ranges, int *out_n_pages, int *out_total_pages);

#ifdef __cplusplus
}
#endif

#endif /* BRIN_H */
