/**
 * @file storage_engine.h
 * @brief 存储引擎接口定义
 *
 * 定义统一存储引擎接口，各数据模型（KV、向量、时序、文档、空间、树）引擎
 * 均需实现此接口。
 */
#ifndef DB_STORAGE_ENGINE_H
#define DB_STORAGE_ENGINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 数据模型枚举
 * ======================================================================== */

/**
 * @brief 数据模型类型枚举
 */
typedef enum {
    MODEL_RELATIONAL = 0,    /**< 关系模型 */
    MODEL_KV = 1,            /**< 键值模型 */
    MODEL_GRAPH = 2,         /**< 图模型 */
    MODEL_VECTOR = 3,        /**< 向量模型 */
    MODEL_TIMESERIES = 4,    /**< 时序模型 */
    MODEL_DOCUMENT = 5,      /**< 文档模型 */
    MODEL_SPATIAL = 6,       /**< 空间模型 */
    MODEL_TREE = 7,          /**< 树模型 */
    MODEL_COUNT = 8,         /**< 模型数量 */
} DataModel;

/**
 * @brief 访问模式枚举
 */
typedef enum {
    ACCESS_MODE_READ = 0,        /**< 只读模式 */
    ACCESS_MODE_WRITE = 1,       /**< 只写模式 */
    ACCESS_MODE_READ_WRITE = 2,  /**< 读写模式 */
} AccessMode;

/**
 * @brief 存储统计信息
 */
typedef struct storage_stats_s {
    uint64_t total_size;      /**< 总大小（字节） */
    uint64_t used_size;       /**< 已使用大小（字节） */
    uint64_t num_objects;     /**< 对象数量 */
    uint64_t num_pages;       /**< 使用页面数 */
    double cache_hit_rate;    /**< 缓存命中率 */
} storage_stats_t;

/* ========================================================================
 * 表/集合模式定义
 * ======================================================================== */

/**
 * @brief 列定义
 */
typedef struct column_def_s {
    const char *name;         /**< 列名 */
    int32_t data_type;        /**< 数据类型 */
    bool not_null;            /**< 是否非空 */
    bool is_primary_key;      /**< 是否主键 */
} column_def_t;

/**
 * @brief 索引描述
 */
typedef struct index_desc_s {
    const char *name;         /**< 索引名 */
    const char **columns;     /**< 列名数组 */
    int32_t num_columns;      /**< 列数量 */
    int32_t index_type;       /**< 索引类型 */
} index_desc_t;

/**
 * @brief 表/集合模式
 */
typedef struct storage_schema_s {
    const char *name;             /**< 名称 */
    DataModel model;              /**< 数据模型 */
    column_def_t *columns;        /**< 列定义数组 */
    int32_t num_columns;          /**< 列数量 */
    index_desc_t *indexes;        /**< 索引定义数组 */
    int32_t num_indexes;          /**< 索引数量 */
} storage_schema_t;

/* ========================================================================
 * 扫描键定义
 * ======================================================================== */

/**
 * @brief 扫描键操作符
 */
typedef enum {
    SCAN_OP_EQ = 0,       /**< 等于 */
    SCAN_OP_NE = 1,       /**< 不等于 */
    SCAN_OP_LT = 2,       /**< 小于 */
    SCAN_OP_LE = 3,       /**< 小于等于 */
    SCAN_OP_GT = 4,       /**< 大于 */
    SCAN_OP_GE = 5,       /**< 大于等于 */
} scan_operator_t;

/**
 * @brief 扫描键
 */
typedef struct scan_key_s {
    const char *column;       /**< 列名 */
    scan_operator_t op;       /**< 操作符 */
    const void *value;        /**< 值 */
    size_t value_len;         /**< 值长度 */
} scan_key_t;

/**
 * @brief 扫描方向
 */
typedef enum {
    FORWARD_SCAN = 0,     /**< 正向扫描 */
    BACKWARD_SCAN = 1,    /**< 反向扫描 */
} ScanDirection;

/**
 * @brief 扫描描述符（不透明类型）
 */
typedef struct scan_desc_s scan_desc_t;

/* ========================================================================
 * 存储引擎操作接口
 * ======================================================================== */

/**
 * @brief 存储引擎操作函数表
 */
typedef struct storage_ops_s {
    const char *name;                      /**< 引擎名称 */
    DataModel model;                       /**< 支持的数据模型 */

    /* 生命周期 */
    int (*init)(const char *data_dir);
    int (*shutdown)(void);

    /* 表操作 */
    int (*table_create)(const char *name, const storage_schema_t *schema);
    void *(*table_open)(const char *name, AccessMode mode);
    int (*table_close)(void *rel);
    int (*table_drop)(const char *name);

    /* 元组操作 */
    int (*tuple_insert)(void *rel, const void *data, size_t len);
    int (*tuple_update)(void *rel, const void *old_data, size_t old_len,
                        const void *new_data, size_t new_len);
    int (*tuple_delete)(void *rel, const void *key, size_t key_len);

    /* 扫描操作 */
    scan_desc_t *(*scan_begin)(void *rel, const scan_key_t *keys, int nkeys,
                               ScanDirection direction);
    int (*scan_next)(scan_desc_t *scan, void *out_data, size_t *out_len);
    int (*scan_end)(scan_desc_t *scan);

    /* 索引操作 */
    int (*index_create)(const char *table, const index_desc_t *index);
    int (*index_drop)(const char *index_name);

    /* 统计 */
    int (*get_stats)(const char *name, storage_stats_t *stats);
} storage_ops_t;

/* ========================================================================
 * 引擎注册与管理
 * ======================================================================== */

/**
 * @brief 获取指定模型的存储引擎操作表
 */
const storage_ops_t *storage_get_engine(DataModel model);

/**
 * @brief 注册存储引擎
 */
int storage_register_engine(DataModel model, const storage_ops_t *ops);

/**
 * @brief 初始化所有存储引擎
 */
int storage_init_all(const char *data_dir);

/**
 * @brief 关闭所有存储引擎
 */
void storage_shutdown_all(void);

/**
 * @brief 获取数据模型名称
 */
const char *storage_model_name(DataModel model);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_ENGINE_H */
