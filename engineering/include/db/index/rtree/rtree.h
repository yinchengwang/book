/*
 * rtree.h
 *
 * Public API for an R-Tree spatial index.
 *
 * R-Tree 是一种空间索引，用于高效存储和查询多维矩形：
 * - 将矩形分组存储在叶节点
 * - 内部节点记录子节点的最小边界矩形（MBR）
 * - 查询时使用 MBR 剪枝加速
 *
 * 应用场景：
 * - 地理信息系统（GIS）
 * - 地图应用
 * - 碰撞检测
 */

#ifndef RTREE_INDEX_H
#define RTREE_INDEX_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 矩形定义（2D）。 */
typedef struct {
    float min_x, min_y;
    float max_x, max_y;
} rect_t;

/* R-Tree opaque 类型。 */
typedef struct rtree rtree_t;

/* 查询回调函数。返回 0 继续遍历，非 0 停止。 */
typedef int (*rtree_callback_fn)(const rect_t *rect, const void *data, void *ctx);

/**
 * 创建 R-Tree。
 *
 * @param max_entries 每个节点最大条目数（建议 9）
 * @return 新建的 R-Tree，失败返回 NULL
 */
rtree_t *rtree_create(uint32_t max_entries);

/**
 * 销毁 R-Tree。
 */
void rtree_drop(rtree_t *tree);

/**
 * 插入矩形和数据。
 *
 * @return 0 成功，-1 失败
 */
int rtree_insert(rtree_t *tree, const rect_t *rect, const void *data);

/**
 * 删除矩形和数据。
 *
 * @return 0 成功删除，-1 未找到
 */
int rtree_delete(rtree_t *tree, const rect_t *rect, const void *data);

/**
 * 矩形查询：返回所有与 query_rect 相交的矩形。
 *
 * @return 匹配的数量
 */
int rtree_search(rtree_t *tree, const rect_t *query_rect,
                 rtree_callback_fn callback, void *ctx);

/**
 * 获取条目数量。
 */
uint32_t rtree_size(const rtree_t *tree);

#ifdef __cplusplus
}
#endif

#endif /* RTREE_INDEX_H */
