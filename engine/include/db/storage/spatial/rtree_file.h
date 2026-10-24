/**
 * @file rtree_file.h
 * @brief R-Tree 持久化存储
 *
 * 支持 R-Tree 索引的页面化持久化存储，对齐 PostgreSQL GiST 格式。
 * 支持 mmap 内存映射和 LRU 页面缓存。
 */
#ifndef DB_RTREE_FILE_H
#define DB_RTREE_FILE_H

#include "rtree.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** R-Tree 文件魔数 */
#define RTREE_FILE_MAGIC 0x52545245  /* "RTRE" */

/** R-Tree 文件版本 */
#define RTREE_FILE_VERSION 1

/** 默认页面大小 */
#define RTREE_PAGE_SIZE 4096

/** 页面缓存最大页数 */
#define RTREE_CACHE_MAX_PAGES 256

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/**
 * @brief R-Tree 文件头
 */
typedef struct rtree_file_header_s {
    uint32_t magic;           /**< 魔数 */
    uint16_t version;         /**< 版本 */
    uint16_t page_size;       /**< 页面大小 */
    uint64_t root_offset;     /**< 根节点偏移 */
    uint32_t root_level;      /**< 根节点层级 */
    uint32_t node_count;      /**< 节点数 */
    uint32_t item_count;      /**< 索引项数 */
} rtree_file_header_t;

/**
 * @brief 页面缓存项
 */
typedef struct rtree_cache_entry_s {
    int32_t page_id;          /**< 页面 ID */
    bool dirty;               /**< 是否脏页 */
    void *data;               /**< 页面数据 */
    struct rtree_cache_entry_s *next;  /**< LRU 链表 */
    struct rtree_cache_entry_s *prev;
} rtree_cache_entry_t;

/**
 * @brief R-Tree 文件存储
 */
typedef struct rtree_file_s {
    char path[512];           /**< 文件路径 */
    FILE *fp;                 /**< 文件句柄 */
    bool mmap_enabled;        /**< 是否启用 mmap */
    void *mmap_base;          /**< mmap 基地址 */
    size_t mmap_size;         /**< mmap 大小 */

    /* 文件元数据 */
    rtree_file_header_t header;

    /* 页面缓存 */
    rtree_cache_entry_t *cache;       /**< 缓存哈希表 */
    uint32_t cache_size;              /**< 缓存大小 */
    rtree_cache_entry_t *lru_head;    /**< LRU 链表头 */
    rtree_cache_entry_t *lru_tail;    /**< LRU 链表尾 */
    uint32_t cache_hits;              /**< 缓存命中 */
    uint32_t cache_misses;            /**< 缓存未命中 */

    /* R-Tree 内存结构 */
    rtree_t *tree;            /**< R-Tree 内存结构 */
} rtree_file_t;

/* ========================================================================
 * 生命周期 API
 * ======================================================================== */

/**
 * @brief 创建 R-Tree 文件存储
 *
 * @param path 文件路径
 * @param page_size 页面大小
 * @return R-Tree 文件句柄，失败返回 NULL
 */
rtree_file_t *rtree_file_create(const char *path, uint32_t page_size);

/**
 * @brief 打开已存在的 R-Tree 文件
 *
 * @param path 文件路径
 * @return R-Tree 文件句柄，失败返回 NULL
 */
rtree_file_t *rtree_file_open(const char *path);

/**
 * @brief 保存并关闭 R-Tree 文件
 *
 * @param file R-Tree 文件句柄
 * @return 0 成功，-1 失败
 */
int rtree_file_close(rtree_file_t *file);

/**
 * @brief 释放 R-Tree 文件句柄（不保存）
 *
 * @param file R-Tree 文件句柄
 */
void rtree_file_free(rtree_file_t *file);

/* ========================================================================
 * 操作 API
 * ======================================================================== */

/**
 * @brief 获取底层 R-Tree
 *
 * @param file R-Tree 文件句柄
 * @return R-Tree 指针
 */
rtree_t *rtree_file_get_tree(rtree_file_t *file);

/**
 * @brief 插入索引项
 *
 * @param file R-Tree 文件句柄
 * @param id 对象 ID
 * @param bbox 边界框
 * @return 0 成功，-1 失败
 */
int rtree_file_insert(rtree_file_t *file, uint64_t id, const bbox_t *bbox);

/**
 * @brief 删除索引项
 *
 * @param file R-Tree 文件句柄
 * @param id 对象 ID
 * @param bbox 边界框
 * @return 0 成功，-1 未找到
 */
int rtree_file_remove(rtree_file_t *file, uint64_t id, const bbox_t *bbox);

/**
 * @brief 边界框查询
 *
 * @param file R-Tree 文件句柄
 * @param query 查询边界框
 * @param results 结果数组
 * @param max_results 最大结果数
 * @return 匹配数量
 */
int rtree_file_search(rtree_file_t *file, const bbox_t *query,
                      rtree_result_t *results, int max_results);

/**
 * @brief KNN 查询
 *
 * @param file R-Tree 文件句柄
 * @param point 查询点
 * @param k 返回数量
 * @param results 结果数组
 * @param max_results 数组容量
 * @return 找到的数量
 */
int rtree_file_knn(rtree_file_t *file, const point_t *point, int k,
                   rtree_result_t *results, int max_results);

/* ========================================================================
 * 统计 API
 * ======================================================================== */

/**
 * @brief 获取文件统计信息
 *
 * @param file R-Tree 文件句柄
 * @param out_nodes 输出节点数
 * @param out_items 输出项数
 * @param out_cache_hits 输出缓存命中
 * @param out_cache_misses 输出缓存未命中
 */
void rtree_file_get_stats(rtree_file_t *file,
                          uint32_t *out_nodes, uint32_t *out_items,
                          uint32_t *out_cache_hits, uint32_t *out_cache_misses);

/* ========================================================================
 * 文件完整性校验 API
 * ======================================================================== */

/**
 * @brief 验证 R-Tree 文件完整性
 *
 * 检查魔数、版本、文件头和数据完整性。
 *
 * @param path 文件路径
 * @return true 完整，false 损坏
 */
bool rtree_file_verify(const char *path);

/**
 * @brief 获取文件损坏原因
 *
 * 在 verify 失败后调用以获取详细错误信息。
 *
 * @param path 文件路径
 * @return 错误描述字符串
 */
const char *rtree_file_get_corruption_reason(const char *path);

/**
 * @brief 尝试修复损坏的 R-Tree 文件
 *
 * @param path 文件路径
 * @param backup_path 备份文件路径（若为 NULL 则不备份）
 * @return 0 修复成功，-1 修复失败
 */
int rtree_file_repair(const char *path, const char *backup_path);

#ifdef __cplusplus
}
#endif

#endif /* DB_RTREE_FILE_H */
