/**
 * @file hilbert.h
 * @brief Hilbert 曲线空间填充曲线实现
 *
 * Hilbert 曲线是一种空间填充曲线，能够保持良好的空间局部性。
 * 支持 2D/3D 坐标与 1D Hilbert 索引之间的相互转换，
 * 用于优化空间查询、KNN 搜索等场景。
 */
#ifndef DB_INDEX_HILBERT_H
#define DB_INDEX_HILBERT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** Hilbert 曲线最大阶数 */
#define HILBERT_MAX_ORDER 32

/** 默认 Hilbert 阶数 */
#define HILBERT_DEFAULT_ORDER 16

/** Hilbert 索引无效值 */
#define HILBERT_INVALID_INDEX UINT64_MAX

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief 2D 点
 */
typedef struct hilbert_point2d_s {
    double x;    /**< X 坐标 */
    double y;    /**< Y 坐标 */
} hilbert_point2d_t;

/**
 * @brief 3D 点
 */
typedef struct hilbert_point3d_s {
    double x;    /**< X 坐标 */
    double y;    /**< Y 坐标 */
    double z;    /**< Z 坐标 */
} hilbert_point3d_t;

/**
 * @brief 边界框
 */
typedef struct hilbert_bbox_s {
    double min_x;
    double min_y;
    double max_x;
    double max_y;
} hilbert_bbox_t;

/**
 * @brief Hilbert 索引项
 */
typedef struct hilbert_record_s {
    uint64_t hilbert_code;    /**< Hilbert 码 */
    uint64_t item_id;         /**< 关联的 item ID */
    hilbert_bbox_t bbox;      /**< 边界框 */
} hilbert_record_t;

/**
 * @brief Hilbert 辅助索引
 */
typedef struct hilbert_index_s {
    hilbert_record_t *records;    /**< 记录数组（按 Hilbert 码排序） */
    uint32_t count;               /**< 记录数 */
    uint32_t capacity;            /**< 容量 */
    uint32_t order;               /**< Hilbert 阶数 */
    hilbert_bbox_t data_bbox;     /**< 数据边界框 */
} hilbert_index_t;

/* ========================================================================
 * 编码/解码 API
 * ======================================================================== */

/**
 * @brief 2D 坐标编码为 Hilbert 索引
 *
 * @param x X 坐标
 * @param y Y 坐标
 * @param order Hilbert 阶数
 * @return Hilbert 索引
 */
uint64_t hilbert_encode2d(double x, double y, uint32_t order);

/**
 * @brief 3D 坐标编码为 Hilbert 索引
 *
 * @param x X 坐标
 * @param y Y 坐标
 * @param z Z 坐标
 * @param order Hilbert 阶数
 * @return Hilbert 索引
 */
uint64_t hilbert_encode3d(double x, double y, double z, uint32_t order);

/**
 * @brief Hilbert 索引解码为 2D 坐标
 *
 * @param h Hilbert 索引
 * @param order Hilbert 阶数
 * @param out_x 输出 X 坐标
 * @param out_y 输出 Y 坐标
 */
void hilbert_decode2d(uint64_t h, uint32_t order, double *out_x, double *out_y);

/**
 * @brief Hilbert 索引解码为 3D 坐标
 *
 * @param h Hilbert 索引
 * @param order Hilbert 阶数
 * @param out_x 输出 X 坐标
 * @param out_y 输出 Y 坐标
 * @param out_z 输出 Z 坐标
 */
void hilbert_decode3d(uint64_t h, uint32_t order,
                      double *out_x, double *out_y, double *out_z);

/* ========================================================================
 * BBox 相关 API
 * ======================================================================== */

/**
 * @brief 计算边界框的 Hilbert 范围
 *
 * @param bbox 边界框
 * @param order Hilbert 阶数
 * @param out_min 输出最小 Hilbert 码
 * @param out_max 输出最大 Hilbert 码
 */
void hilbert_bbox_range(const hilbert_bbox_t *bbox, uint32_t order,
                        uint64_t *out_min, uint64_t *out_max);

/**
 * @brief 计算边界框中心的 Hilbert 码
 *
 * @param bbox 边界框
 * @param order Hilbert 阶数
 * @return Hilbert 码
 */
uint64_t hilbert_bbox_center(const hilbert_bbox_t *bbox, uint32_t order);

/* ========================================================================
 * 索引管理 API
 * ======================================================================== */

/**
 * @brief 创建 Hilbert 辅助索引
 *
 * @param capacity 预估容量
 * @param order Hilbert 阶数
 * @return 索引句柄，失败返回 NULL
 */
hilbert_index_t *hilbert_index_create(uint32_t capacity, uint32_t order);

/**
 * @brief 释放 Hilbert 辅助索引
 *
 * @param index 索引句柄
 */
void hilbert_index_destroy(hilbert_index_t *index);

/**
 * @brief 向 Hilbert 索引添加记录
 *
 * @param index 索引句柄
 * @param item_id 关联的 item ID
 * @param bbox 边界框
 * @return 0 成功，-1 失败
 */
int hilbert_index_add(hilbert_index_t *index, uint64_t item_id,
                      const hilbert_bbox_t *bbox);

/**
 * @brief 构建/排序 Hilbert 索引
 *
 * 按 Hilbert 码排序后，相同区域的 items 会聚在一起。
 *
 * @param index 索引句柄
 */
void hilbert_index_build(hilbert_index_t *index);

/**
 * @brief 在 Hilbert 索引中搜索范围内的 items
 *
 * @param index 索引句柄
 * @param query 查询边界框
 * @param results 结果数组（调用者分配）
 * @param max_results 最大结果数
 * @return 匹配数量
 */
uint32_t hilbert_index_range_query(const hilbert_index_t *index,
                                   const hilbert_bbox_t *query,
                                   uint64_t *results,
                                   uint32_t max_results);

/**
 * @brief 在 Hilbert 索引中搜索最近的 k 个 items
 *
 * @param index 索引句柄
 * @param point 查询点
 * @param k 返回数量
 * @param results 结果数组（调用者分配，按距离排序）
 * @param max_results 数组容量
 * @return 找到的数量
 */
uint32_t hilbert_index_knn(const hilbert_index_t *index,
                           const hilbert_point2d_t *point, int k,
                           hilbert_record_t *results,
                           uint32_t max_results);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 根据数据范围自动选择 Hilbert 阶数
 *
 * @param width 数据宽度
 * @param height 数据高度
 * @return 推荐阶数
 */
uint32_t hilbert_auto_order(double width, double height);

/**
 * @brief 计算两个 Hilbert 码之间的距离
 *
 * 用于螺旋搜索时的距离计算。
 *
 * @param code1 Hilbert 码 1
 * @param code2 Hilbert 码 2
 * @return 距离（近似）
 */
uint64_t hilbert_distance(uint64_t code1, uint64_t code2);

/**
 * @brief 检查 Hilbert 码是否在指定范围内
 *
 * @param code Hilbert 码
 * @param min 最小码
 * @param max 最大码
 * @return true 在范围内
 */
bool hilbert_in_range(uint64_t code, uint64_t min, uint64_t max);

/* ========================================================================
 * 持久化 API
 * ======================================================================== */

/** Hilbert 索引文件魔数 */
#define HILBERT_INDEX_MAGIC 0x48494C42  /* "HILB" */

/** Hilbert 索引文件版本 */
#define HILBERT_INDEX_VERSION 1

/**
 * @brief 保存 Hilbert 索引到文件
 *
 * @param index 索引句柄
 * @param path 文件路径
 * @return 0 成功，-1 失败
 */
int hilbert_index_save(const hilbert_index_t *index, const char *path);

/**
 * @brief 从文件加载 Hilbert 索引
 *
 * @param path 文件路径
 * @return 索引句柄，失败返回 NULL
 */
hilbert_index_t *hilbert_index_load(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DB_INDEX_HILBERT_H */
