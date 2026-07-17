/**
 * @file rtree.h
 * @brief R-Tree 空间索引实现
 *
 * R-Tree 是一种用于空间索引的树形数据结构，
 * 支持高效的矩形查询和最近邻搜索。
 */
#ifndef DB_STORAGE_SPATIAL_RTREE_H
#define DB_STORAGE_SPATIAL_RTREE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 几何类型定义
 * ======================================================================== */

/* 如果调用者已经包含了定义这些类型的头文件，使用它；
 * 否则在此处定义（通过保护宏避免重复定义） */
#ifndef POINT_T_DEFINED
#define POINT_T_DEFINED
typedef struct {
    double x;
    double y;
} point_t;
#endif

#ifndef BBOX_T_DEFINED
#define BBOX_T_DEFINED
typedef struct {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} bbox_t;
#endif

/* ========================================================================
 * R-Tree 节点
 * ======================================================================== */

/** R-Tree 节点 */
typedef struct rtree_node_s rtree_node_t;

/** R-Tree 节点结构 */
struct rtree_node_s {
    bool is_leaf;                    /**< 是否叶子节点 */
    bbox_t bbox;                     /**< 节点边界框 */
    uint32_t num_entries;            /**< 条目数量 */

    union {
        /** 叶子节点数据 */
        struct {
            uint64_t *ids;           /**< 对象 ID 数组 */
            bbox_t *bboxes;          /**< 边界框数组 */
        } leaf;
        /** 非叶子节点数据 */
        struct {
            rtree_node_t **children; /**< 子节点指针数组 */
        } internal;
    } data;
};

/* ========================================================================
 * R-Tree 结构
 * ======================================================================== */

/** R-Tree 搜索结果 */
typedef struct rtree_result_s {
    uint64_t id;            /**< 对象 ID */
    bbox_t bbox;            /**< 对象边界框 */
    double distance;       /**< 到查询点的距离（KNN 查询时有效） */
} rtree_result_t;

/** R-Tree 统计信息 */
typedef struct rtree_stats_s {
    uint32_t num_nodes;     /**< 节点数 */
    uint32_t num_items;     /**< 索引项数 */
    uint32_t height;        /**< 树高度 */
    uint32_t max_fill;     /**< 最大填充数 */
} rtree_stats_t;

/** R-Tree 结构 */
typedef struct rtree_s {
    rtree_node_t *root;             /**< 根节点 */
    uint32_t max_entries;           /**< 每个节点最大条目数 */
    uint32_t min_entries;           /**< 每个节点最小条目数 */
    uint32_t num_items;             /**< 索引项总数 */
    uint32_t height;                /**< 树高度 */
} rtree_t;

/* ========================================================================
 * R-Tree API
 * ======================================================================== */

/**
 * @brief 创建 R-Tree
 * @param max_entries 每个节点最大条目数（默认 16）
 * @return R-Tree 指针，失败返回 NULL
 */
rtree_t *rtree_create(int max_entries);

/**
 * @brief 释放 R-Tree
 * @param tree R-Tree 指针
 */
void rtree_free(rtree_t *tree);

/**
 * @brief 插入索引项
 * @param tree R-Tree
 * @param id 对象 ID
 * @param bbox 边界框
 * @return 0 成功，-1 失败
 */
int rtree_insert(rtree_t *tree, uint64_t id, const bbox_t *bbox);

/**
 * @brief 删除索引项
 * @param tree R-Tree
 * @param id 对象 ID
 * @param bbox 边界框
 * @return 0 成功，-1 未找到
 */
int rtree_remove(rtree_t *tree, uint64_t id, const bbox_t *bbox);

/**
 * @brief 边界框搜索
 * @param tree R-Tree
 * @param query 查询边界框
 * @param results 结果数组（调用者分配）
 * @param max_results 最大结果数
 * @return 匹配数量
 */
int rtree_search(rtree_t *tree, const bbox_t *query,
                 rtree_result_t *results, int max_results);

/**
 * @brief 最近邻搜索
 * @param tree R-Tree
 * @param point 查询点
 * @param k 返回的最近邻数量
 * @param results 结果数组（调用者分配）
 * @param max_results 数组容量
 * @return 找到的数量
 */
int rtree_knn(rtree_t *tree, const point_t *point, int k,
              rtree_result_t *results, int max_results);

/**
 * @brief 保存 R-Tree 到文件
 * @param tree R-Tree
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int rtree_save(rtree_t *tree, const char *path);

/**
 * @brief 从文件加载 R-Tree
 * @param path 文件路径
 * @return R-Tree 指针，失败返回 NULL
 */
rtree_t *rtree_load(const char *path);

/**
 * @brief 获取统计信息
 * @param tree R-Tree
 * @param stats 输出统计信息
 */
void rtree_stats(rtree_t *tree, rtree_stats_t *stats);

/* ========================================================================
 * 几何工具函数
 * ======================================================================== */

/**
 * @brief 创建边界框
 */
bbox_t bbox_create(double min_x, double min_y, double max_x, double max_y);

/**
 * @brief 检查点是否在边界框内
 */
bool bbox_contains_point(const bbox_t *bbox, const point_t *point);

/**
 * @brief 检查两个边界框是否相交
 */
bool bbox_intersects(const bbox_t *a, const bbox_t *b);

/**
 * @brief 计算边界框面积
 */
double bbox_area(const bbox_t *bbox);

/**
 * @brief 计算边界框扩大后的面积
 */
double bbox_area_enlarged(const bbox_t *bbox, const bbox_t *other);

/**
 * @brief 计算点与边界框的最小距离
 */
double bbox_point_distance(const bbox_t *bbox, const point_t *point);

/**
 * @brief 合并两个边界框
 */
bbox_t bbox_union(const bbox_t *a, const bbox_t *b);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_SPATIAL_RTREE_H */
