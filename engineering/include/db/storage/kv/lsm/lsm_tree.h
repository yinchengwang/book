/**
 * @file lsm_tree.h
 * @brief LSM-Tree 存储引擎接口
 *
 * Phase12 - 实现 LSM-Tree 架构，追赶 RocksDB 水平。
 *
 * 设计目标：
 * - 内存组件：MemTable (跳表或 BTree)
 * - 磁盘组件：SSTable (L0, L1, L2, ...)
 * - 写入路径：WAL + MemTable → 刷盘 → L0 SSTable → Compaction
 * - 读取路径：MemTable → 最新 SSTable → 次新 SSTable → ...
 * - 布隆过滤器：减少无效 SSTable 访问
 *
 * Leveled Compaction:
 * - L0: 新刷盘的 SSTable，可能重叠
 * - L1+: 每层大小上限指数增长，不可重叠
 */
#ifndef DB_STORAGE_KV_LSM_TREE_H
#define DB_STORAGE_KV_LSM_TREE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 最大层数 */
#define LSM_MAX_LEVELS 8

/** 每层大小倍数 */
#define LSM_SIZE_RATIO 10

/** 默认 MemTable 大小 (64MB) */
#define LSM_DEFAULT_MEMTABLE_SIZE (64 * 1024 * 1024)

/** 默认 SSTable 大小 (256MB) */
#define LSM_DEFAULT_SSTABLE_SIZE (256 * 1024 * 1024)

/** 布隆过滤器位数组大小 */
#define LSM_BLOOM_FILTER_SIZE (1024 * 1024)

/** 布隆过滤器哈希函数数量 */
#define LSM_BLOOM_FILTER_HASH_COUNT 7

/** 最大键长度 */
#define LSM_MAX_KEY_SIZE 8192

/** 最大值长度 */
#define LSM_MAX_VALUE_SIZE (16 * 1024 * 1024)  /* 16MB */

/* ========================================================================
 * 类型定义
 * ======================================================================== */

/** 操作类型 */
typedef enum {
    LSM_OP_PUT = 0,
    LSM_OP_DELETE = 1
} lsm_operation_t;

/** KV 对 */
typedef struct {
    uint64_t seq;             /**< 序列号 */
    lsm_operation_t op;       /**< 操作类型 */
    uint32_t key_size;         /**< 键长度 */
    uint32_t value_size;       /**< 值长度 */
    void *key;                 /**< 键数据 */
    void *value;               /**< 值数据 */
} lsm_entry_t;

/** 布隆过滤器 */
typedef struct {
    uint8_t *bits;            /**< 位数组 */
    size_t size;               /**< 位数组大小（字节）*/
    size_t num_hashes;         /**< 哈希函数数量 */
} lsm_bloom_filter_t;

/** SSTable 元数据 */
typedef struct {
    uint64_t file_id;         /**< 文件 ID */
    char path[512];             /**< 文件路径 */
    uint64_t min_key;          /**< 最小键 */
    uint64_t max_key;          /**< 最大键 */
    uint64_t min_seq;          /**< 最小序列号 */
    uint64_t max_seq;          /**< 最大序列号 */
    size_t file_size;          /**< 文件大小 */
    lsm_bloom_filter_t *bloom; /**< 布隆过滤器 */
    bool being_compacted;       /**< 是否正在压缩 */
} lsm_sstable_meta_t;

/** 层信息 */
typedef struct {
    uint32_t level;            /**< 层编号 */
    lsm_sstable_meta_t **tables; /**< SSTable 数组 */
    uint32_t table_count;      /**< SSTable 数量 */
    size_t size_limit;          /**< 大小上限 */
} lsm_level_t;

/** LSM 树状态 */
typedef enum {
    LSM_STATE_NORMAL = 0,
    LSM_STATE_COMPACTING = 1,
    LSM_STATE_FLUSHING = 2,
    LSM_STATE_ERROR = 3
} lsm_state_t;

/** LSM 树配置 */
typedef struct {
    const char *data_dir;         /**< 数据目录 */
    size_t memtable_size;           /**< MemTable 大小 */
    size_t sstable_size;           /**< SSTable 大小 */
    uint32_t num_levels;          /**< 层数 */
    size_t size_ratio;             /**< 每层大小倍数 */
    bool enable_bloom_filter;      /**< 启用布隆过滤器 */
    bool enable_cache;             /**< 启用块缓存 */
    uint32_t cache_size;           /**< 缓存大小 */
} lsm_config_t;

/** LSM 树不透明类型 */
typedef struct lsm_tree lsm_tree_t;

/* ========================================================================
 * 生命周期
 * ======================================================================== */

/**
 * @brief 创建 LSM 树
 *
 * @param config 配置
 * @return 成功返回 LSM 树指针，失败返回 NULL
 */
lsm_tree_t *lsm_tree_create(const lsm_config_t *config);

/**
 * @brief 打开已存在的 LSM 树
 *
 * @param data_dir 数据目录
 * @return 成功返回 LSM 树指针，失败返回 NULL
 */
lsm_tree_t *lsm_tree_open(const char *data_dir);

/**
 * @brief 关闭 LSM 树
 *
 * @param tree LSM 树
 */
void lsm_tree_close(lsm_tree_t *tree);

/**
 * @brief 获取配置
 *
 * @param tree LSM 树
 * @return 配置指针
 */
const lsm_config_t *lsm_tree_get_config(const lsm_tree_t *tree);

/**
 * @brief 获取当前状态
 */
lsm_state_t lsm_tree_get_state(const lsm_tree_t *tree);

/* ========================================================================
 * 写入操作
 * ======================================================================== */

/**
 * @brief 插入或更新键值对
 *
 * @param tree LSM 树
 * @param key 键
 * @param key_size 键长度
 * @param value 值
 * @param value_size 值长度
 * @return 0 成功，非 0 失败
 */
int lsm_tree_put(lsm_tree_t *tree,
                const void *key, size_t key_size,
                const void *value, size_t value_size);

/**
 * @brief 删除键
 *
 * @param tree LSM 树
 * @param key 键
 * @param key_size 键长度
 * @return 0 成功
 */
int lsm_tree_delete(lsm_tree_t *tree,
                   const void *key, size_t key_size);

/**
 * @brief 批量写入
 *
 * @param tree LSM 树
 * @param entries 条目数组
 * @param count 条目数量
 * @return 成功写入的数量
 */
size_t lsm_tree_batch_put(lsm_tree_t *tree,
                         const lsm_entry_t *entries,
                         size_t count);

/**
 * @brief 刷 MemTable 到磁盘
 *
 * @param tree LSM 树
 * @return 0 成功
 */
int lsm_tree_flush(lsm_tree_t *tree);

/* ========================================================================
 * 读取操作
 * ======================================================================== */

/**
 * @brief 获取值
 *
 * @param tree LSM 树
 * @param key 键
 * @param key_size 键长度
 * @param out_value 输出值（调用者负责释放）
 * @param out_size 输出值大小
 * @return 0 成功，1 不存在，负值错误
 */
int lsm_tree_get(lsm_tree_t *tree,
                 const void *key, size_t key_size,
                 void **out_value, size_t *out_size);

/**
 * @brief 检查键是否存在
 *
 * @param tree LSM 树
 * @param key 键
 * @param key_size 键长度
 * @return true 存在
 */
bool lsm_tree_exists(lsm_tree_t *tree,
                    const void *key, size_t key_size);

/* ========================================================================
 * 迭代器
 * ======================================================================== */

/** LSM 树迭代器 */
typedef struct lsm_iterator lsm_iterator_t;

/**
 * @brief 创建迭代器
 *
 * @param tree LSM 树
 * @param start_key 起始键（NULL 表示从头开始）
 * @param start_size 起始键长度
 * @param end_key 结束键（NULL 表示到结尾）
 * @param end_size 结束键长度
 * @return 迭代器
 */
lsm_iterator_t *lsm_tree_create_iterator(lsm_tree_t *tree,
                                        const void *start_key, size_t start_size,
                                        const void *end_key, size_t end_size);

/**
 * @brief 移动到下一个
 *
 * @param iter 迭代器
 * @return 0 成功，1 结束
 */
int lsm_iterator_next(lsm_iterator_t *iter);

/**
 * @brief 获取当前键
 */
const void *lsm_iterator_key(const lsm_iterator_t *iter);

/**
 * @brief 获取当前键长度
 */
size_t lsm_iterator_key_size(const lsm_iterator_t *iter);

/**
 * @brief 获取当前值
 */
const void *lsm_iterator_value(const lsm_iterator_t *iter);

/**
 * @brief 获取当前值长度
 */
size_t lsm_iterator_value_size(const lsm_iterator_t *iter);

/**
 * @brief 销毁迭代器
 */
void lsm_iterator_destroy(lsm_iterator_t *iter);

/* ========================================================================
 * Compaction
 * ======================================================================== */

/**
 * @brief 执行 Compaction
 *
 * @param tree LSM 树
 * @param level 目标层（0 表示自动选择）
 * @return 0 成功
 */
int lsm_tree_compact(lsm_tree_t *tree, uint32_t level);

/**
 * @brief 检查是否需要 Compaction
 *
 * @param tree LSM 树
 * @return true 需要
 */
bool lsm_tree_needs_compaction(const lsm_tree_t *tree);

/**
 * @brief 获取某层的 SSTable 数量
 */
uint32_t lsm_tree_get_level_count(const lsm_tree_t *tree, uint32_t level);

/**
 * @brief 获取总大小
 */
size_t lsm_tree_get_total_size(const lsm_tree_t *tree);

/**
 * @brief 获取键数量
 */
uint64_t lsm_tree_get_num_keys(const lsm_tree_t *tree);

/* ========================================================================
 * 统计信息
 * ======================================================================== */

/** LSM 树统计信息 */
typedef struct {
    uint64_t num_keys;          /**< 键数量 */
    size_t memtable_size;        /**< MemTable 大小 */
    size_t total_size;          /**< 总大小 */
    uint32_t num_levels;        /**< 层数 */
    uint32_t num_sstables;      /**< SSTable 总数 */
    uint32_t num_l0_sstables;   /**< L0 SSTable 数 */
    size_t cache_size;          /**< 缓存大小 */
    double cache_hit_rate;       /**< 缓存命中率 */
    uint64_t compaction_count;   /**< Compaction 次数 */
    uint64_t flush_count;       /**< 刷盘次数 */
} lsm_stats_t;

/**
 * @brief 获取统计信息
 */
void lsm_tree_get_stats(const lsm_tree_t *tree, lsm_stats_t *stats);

/* ========================================================================
 * 布隆过滤器
 * ======================================================================== */

/**
 * @brief 创建布隆过滤器
 */
lsm_bloom_filter_t *lsm_bloom_create(size_t size, size_t num_hashes);

/**
 * @brief 添加键到布隆过滤器
 */
void lsm_bloom_add(lsm_bloom_filter_t *bloom,
                   const void *key, size_t key_size);

/**
 * @brief 检查键可能存在
 */
bool lsm_bloom_might_contain(const lsm_bloom_filter_t *bloom,
                             const void *key, size_t key_size);

/**
 * @brief 销毁布隆过滤器
 */
void lsm_bloom_destroy(lsm_bloom_filter_t *bloom);

/**
 * @brief 序列化布隆过滤器
 */
int lsm_bloom_serialize(const lsm_bloom_filter_t *bloom,
                        void **out_data, size_t *out_size);

/**
 * @brief 反序列化布隆过滤器
 */
lsm_bloom_filter_t *lsm_bloom_deserialize(const void *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_KV_LSM_TREE_H */
