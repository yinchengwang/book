/*
 * gist.h — GiST (Generalized Search Tree) 公共头文件
 *
 * ============================================================
 * 概述
 * ============================================================
 * GiST 是 B-Tree 的通用化版本，支持任意数据类型。
 * 用户通过实现操作符接口来定义具体行为。
 *
 * 特性：
 * - 支持任意数据类型的空间索引扩展
 * - 提供统一的树形索引结构，可适配 R-Tree、全文搜索等
 * - 支持范围查询和 KNN 查询
 *
 * ============================================================
 * 使用示例
 * ============================================================
 *
 *   // 定义矩形操作符
 *   gist_ops_t rect_ops = {
 *       .union_fn = rect_union,
 *       .consistent_fn = rect_intersects,
 *       .distance_fn = rect_distance,
 *       .free_fn = free
 *   };
 *
 *   gist_index_t *idx = gist_create(&rect_ops, sizeof(rect_t));
 *   gist_insert(idx, &rect1, value1);
 *
 *   void *results[100];
 *   int count = 0;
 *   gist_range_query(idx, &query_rect, results, &count, 100);
 *
 *   gist_destroy(idx);
 */

#ifndef DB_INDEX_GIST_H
#define DB_INDEX_GIST_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 前向声明 ── */
typedef struct gist_index gist_index_t;
typedef struct gist_node gist_node_t;

/* ── 操作符接口 ── */
typedef void (*gist_union_fn)(const void **children, int count, void *result);
typedef int (*gist_consistent_fn)(const void *bbox, const void *query);
typedef float (*gist_distance_fn)(const void *bbox, const void *point);
typedef int (*gist_size_fn)(const void *bbox);
typedef void (*gist_free_fn)(void *ptr);
typedef void *(*gist_copy_fn)(const void *bbox);

/* ── GiST 操作符集合 ── */
typedef struct gist_ops {
    gist_union_fn union_fn;           /* 合并子节点边界框 */
    gist_consistent_fn consistent_fn;   /* 判断是否一致 */
    gist_distance_fn distance_fn;       /* 计算距离（可选） */
    gist_size_fn size_fn;              /* 计算边界框大小 */
    gist_free_fn free_fn;             /* 释放边界框 */
    gist_copy_fn copy_fn;             /* 复制边界框 */
} gist_ops_t;

/* ── 核心 API ── */

/*
 * gist_create — 创建 GiST 索引
 *
 * @param ops 操作符集合
 * @param bbox_size 边界框结构大小
 * @return GiST 索引指针，失败返回 NULL
 */
gist_index_t *gist_create(const gist_ops_t *ops, int bbox_size);

/*
 * gist_set_fill_factor — 设置填充因子
 *
 * @param idx GiST 索引
 * @param fill_factor 填充因子 (0.0-1.0)
 */
void gist_set_fill_factor(gist_index_t *idx, float fill_factor);

/*
 * gist_insert — 插入记录
 *
 * @param idx GiST 索引
 * @param bbox 边界框
 * @param value 值
 * @return 0 成功，-1 失败
 */
int gist_insert(gist_index_t *idx, const void *bbox, const void *value);

/*
 * gist_range_query — 范围查询
 *
 * @param idx GiST 索引
 * @param query_bbox 查询边界框
 * @param results 输出：匹配的结果数组
 * @param count 输出：结果数量
 * @param max_results 最大结果数
 * @return 0 成功，-1 失败
 */
int gist_range_query(gist_index_t *idx, const void *query_bbox,
                    void **results, int *count, int max_results);

/*
 * gist_knn_query — KNN 查询
 *
 * @param idx GiST 索引
 * @param point 查询点
 * @param k 返回数量
 * @param results 输出：匹配的结果数组
 * @param distances 输出：对应的距离
 * @return 0 成功，-1 失败
 */
int gist_knn_query(gist_index_t *idx, const void *point, int k,
                  void **results, float *distances);

/*
 * gist_delete — 删除记录
 *
 * @param idx GiST 索引
 * @param bbox 边界框
 * @param value 值
 * @return 0 成功，-1 未找到
 */
int gist_delete(gist_index_t *idx, const void *bbox, const void *value);

/*
 * gist_stats — 获取索引统计信息
 *
 * @param idx GiST 索引
 * @param out_n_nodes 输出：节点数
 * @param out_height 输出：树高度
 * @param out_n_entries 输出：条目数
 */
void gist_stats(const gist_index_t *idx, int *out_n_nodes, int *out_height, int *out_n_entries);

/*
 * gist_destroy — 销毁 GiST 索引
 *
 * @param idx GiST 索引
 */
void gist_destroy(gist_index_t *idx);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_GIST_H */
