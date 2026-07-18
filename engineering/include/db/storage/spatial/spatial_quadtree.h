/**
 * @file spatial_quadtree.h
 * @brief QuadTree 空间索引头文件
 *
 * 实现 QuadTree 索引用于空间数据的高效查询
 */
#ifndef DB_SPATIAL_QUADTREE_H
#define DB_SPATIAL_QUADTREE_H

#include "db/storage/spatial/spatial_engine.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * QuadTree 节点
 * ======================================================================== */

/** QuadTree 节点 */
typedef struct QuadTreeNode_s {
    bbox_t boundary;               /**< 节点边界 */
    uint64_t *object_ids;          /**< 对象 ID 数组（叶子节点） */
    size_t num_objects;            /**< 对象数量 */
    size_t capacity;               /**< 容量 */

    struct QuadTreeNode_s *nw;     /**< 西北子节点 */
    struct QuadTreeNode_s *ne;     /**< 东北子节点 */
    struct QuadTreeNode_s *sw;     /**< 西南子节点 */
    struct QuadTreeNode_s *se;     /**< 东南子节点 */

    bool is_leaf;                  /**< 是否叶子节点 */
} QuadTreeNode;

/** QuadTree 结构 */
typedef struct QuadTree_s {
    QuadTreeNode *root;            /**< 根节点 */
    uint32_t max_depth;            /**< 最大深度 */
    uint32_t max_capacity;         /**< 节点最大容量 */
    uint32_t num_objects;          /**< 对象总数 */
} QuadTree;

/** QuadTree 搜索结果 */
typedef struct QuadTreeResult_s {
    uint64_t *ids;                 /**< 对象 ID 数组 */
    size_t num_results;            /**< 结果数量 */
    size_t capacity;               /**< 容量 */
} QuadTreeResult;

/* ========================================================================
 * QuadTree API
 * ======================================================================== */

/**
 * @brief 创建 QuadTree
 * @param boundary 空间边界
 * @param max_capacity 节点最大容量（默认 4）
 * @param max_depth 最大深度（默认 8）
 * @return QuadTree 指针
 */
QuadTree *quadtree_create(const bbox_t *boundary,
                         uint32_t max_capacity,
                         uint32_t max_depth);

/**
 * @brief 销毁 QuadTree
 */
void quadtree_destroy(QuadTree *tree);

/**
 * @brief 插入对象
 * @param tree QuadTree
 * @param id 对象 ID
 * @param bbox 对象边界框
 * @return 0 成功，-1 失败
 */
int quadtree_insert(QuadTree *tree, uint64_t id, const bbox_t *bbox);

/**
 * @brief 删除对象
 * @param tree QuadTree
 * @param id 对象 ID
 * @param bbox 对象边界框
 * @return 0 成功，-1 未找到
 */
int quadtree_remove(QuadTree *tree, uint64_t id, const bbox_t *bbox);

/**
 * @brief 边界框查询
 * @param tree QuadTree
 * @param query 查询边界框
 * @return 搜索结果
 */
QuadTreeResult *quadtree_search(QuadTree *tree, const bbox_t *query);

/**
 * @brief KNN 查询
 * @param tree QuadTree
 * @param point 查询点
 * @param k 返回数量
 * @return 搜索结果（按距离排序）
 */
QuadTreeResult *quadtree_knn(QuadTree *tree, const point_t *point, int k);

/**
 * @brief 释放搜索结果
 */
void quadtree_result_free(QuadTreeResult *result);

/**
 * @brief 获取 QuadTree 统计信息
 */
void quadtree_stats(QuadTree *tree, storage_stats_t *stats);

/**
 * @brief 保存 QuadTree 到文件
 */
int quadtree_save(QuadTree *tree, const char *path);

/**
 * @brief 从文件加载 QuadTree
 */
QuadTree *quadtree_load(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DB_SPATIAL_QUADTREE_H */
