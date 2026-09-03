/**
 * @file mm_storage.h
 * @brief 多模态存储管理器头文件
 *
 * 提供统一的多模态数据库存储管理接口，支持关系、KV、图、向量、
 * 时序、文档、空间、树等多种数据模型的统一管理。
 */
#ifndef DB_MM_STORAGE_H
#define DB_MM_STORAGE_H

#include "storage_engine.h"
#include "errors.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

struct mm_context_s;
typedef struct mm_context_s mm_context_t;

/* ========================================================================
 * 查询条件
 * ======================================================================== */

/**
 * @brief 查询条件
 */
typedef struct query_condition_s {
    const char *column;        /**< 列名 */
    scan_operator_t op;        /**< 操作符 */
    const void *value;         /**< 值 */
    size_t value_len;          /**< 值长度 */
} query_condition_t;

/**
 * @brief 查询规格
 */
typedef struct query_spec_s {
    query_condition_t *conditions;    /**< 条件数组 */
    int num_conditions;               /**< 条件数量 */
    uint64_t limit;                   /**< 限制数量 */
    uint64_t offset;                  /**< 偏移量 */
} query_spec_t;

/* ========================================================================
 * 上下文管理 API
 * ======================================================================== */

/**
 * @brief 初始化多模态存储管理器
 */
int mm_storage_init(const char *data_dir);

/**
 * @brief 关闭多模态存储管理器
 */
int mm_storage_shutdown(void);

/**
 * @brief 获取当前上下文
 */
mm_context_t *mm_get_context(void);

/**
 * @brief 检查是否已初始化
 */
bool mm_is_initialized(void);

/* ========================================================================
 * 模型操作 API
 * ======================================================================== */

/**
 * @brief 创建数据模型存储
 */
int mm_create_model(const char *name, DataModel model, const storage_schema_t *schema);

/**
 * @brief 打开数据模型
 */
void *mm_open_model(const char *name, DataModel model, AccessMode mode);

/**
 * @brief 关闭数据模型
 */
int mm_close_model(void *handle);

/**
 * @brief 删除数据模型
 */
int mm_drop_model(const char *name, DataModel model);

/**
 * @brief 检查模型是否存在
 */
bool mm_model_exists(const char *name, DataModel model);

/* ========================================================================
 * 数据操作 API
 * ======================================================================== */

/**
 * @brief 插入数据
 */
int mm_insert(void *handle, const void *data, size_t len);

/**
 * @brief 更新数据
 */
int mm_update(void *handle, const void *old_data, size_t old_len,
              const void *new_data, size_t new_len);

/**
 * @brief 删除数据
 */
int mm_delete(void *handle, const void *key, size_t key_len);

/**
 * @brief 获取单个数据
 */
int mm_get(void *handle, const void *key, size_t key_len,
           void **out_data, size_t *out_len);

/* ========================================================================
 * 扫描操作 API
 * ======================================================================== */

/**
 * @brief 开始扫描
 */
scan_desc_t *mm_scan_begin(void *handle, const scan_key_t *keys, int nkeys,
                           ScanDirection direction);

/**
 * @brief 获取下一条
 */
int mm_scan_next(scan_desc_t *scan, void *out_data, size_t *out_len);

/**
 * @brief 结束扫描
 */
int mm_scan_end(scan_desc_t *scan);

/* ========================================================================
 * 统计信息 API
 * ======================================================================== */

/**
 * @brief 获取引擎统计信息
 */
int mm_get_engine_stats(DataModel model, storage_stats_t *stats);

/**
 * @brief 获取模型统计信息
 */
int mm_get_model_stats(const char *name, DataModel model, storage_stats_t *stats);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取数据模型目录名
 */
const char *mm_get_model_dir(DataModel model);

/**
 * @brief 获取模型路径
 */
int mm_get_model_path(const char *name, DataModel model,
                      char *out_path, size_t path_size);

/**
 * @brief 获取最后错误
 */
db_error_t *mm_get_error(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_MM_STORAGE_H */
