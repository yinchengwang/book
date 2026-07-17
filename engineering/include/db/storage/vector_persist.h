/**
 * @file vector_persist.h
 * @brief 向量索引持久化层接口
 *
 * 提供向量索引的持久化存储，让向量数据可以从磁盘恢复：
 * - VectorSegment 分段存储
 * - 对接 Buffer Pool 页面缓存
 * - WAL 日志记录与回放
 * - 支持 HNSW/DiskANN/IVF 索引持久化
 */
#ifndef VECTOR_PERSIST_H
#define VECTOR_PERSIST_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** 向量段文件魔数 "VSEG" */
#define VECTOR_SEGMENT_MAGIC 0x56534547

/** 向量段版本 */
#define VECTOR_SEGMENT_VERSION 1

/** 向量段文件扩展名 */
#define VECTOR_SEGMENT_EXT ".vseg"

/** 向量段元数据页扩展名 */
#define VECTOR_SEGMENT_META_EXT ".vseg.meta"

/** 最大段大小 (MB)，超过则创建新段 */
#define VECTOR_SEGMENT_MAX_SIZE_MB 256

/** 默认段向量容量 */
#define VECTOR_SEGMENT_DEFAULT_CAPACITY 1000000

/** 元数据页大小 */
#define VECTOR_SEGMENT_META_PAGE_SIZE 4096

/** WAL 操作类型 */
typedef enum vector_wal_op_type_s {
    VECTOR_WAL_OP_INSERT = 1,      /**< 插入向量 */
    VECTOR_WAL_OP_DELETE = 2,      /**< 删除向量 */
    VECTOR_WAL_OP_UPDATE = 3,      /**< 更新向量 */
    VECTOR_WAL_OP_INDEX_BUILD = 4, /**< 索引构建完成 */
    VECTOR_WAL_OP_CHECKPOINT = 5   /**< 检查点 */
} vector_wal_op_type_t;

/** 索引类型 */
typedef enum vector_index_type_s {
    VECTOR_INDEX_NONE = 0,    /**< 无索引 */
    VECTOR_INDEX_HNSW = 1,    /**< HNSW 图索引 */
    VECTOR_INDEX_DISKANN = 2, /**< DiskANN 图索引 */
    VECTOR_INDEX_IVF = 3      /**< IVF 倒排索引 */
} vector_index_type_t;

/* ========================================================================
 * 元数据页格式 (A.1.4)
 * ======================================================================== */

/**
 * @brief 向量段元数据页头
 */
typedef struct vector_segment_meta_header_s {
    uint32_t magic;           /**< 魔数校验 */
    uint32_t version;         /**< 版本号 */
    int32_t dimension;        /**< 向量维度 */
    int32_t num_vectors;      /**< 当前向量数 */
    int32_t capacity;         /**< 容量上限 */
    int64_t created_at;       /**< 创建时间戳 */
    int64_t modified_at;      /**< 修改时间戳 */
    int32_t index_type;       /**< 索引类型 */
    int32_t num_data_pages;   /**< 数据页数 */
    int32_t num_index_pages;  /**< 索引页数 */
    uint64_t lsn;             /**< 最后 LSN */
    uint32_t checksum;        /**< 元数据校验和 */
    char reserved[60];        /**< 保留空间 */
} vector_segment_meta_header_t;

/* ========================================================================
 * 段结构定义 (A.1.3)
 * ======================================================================== */

/**
 * @brief 向量段
 *
 * 存储一段连续向量数据及其元数据
 */
typedef struct vector_segment_s {
    int32_t segment_id;       /**< 段 ID */
    char data_dir[512];       /**< 数据目录 */
    char segment_path[512];   /**< 段文件路径 */

    /* 元数据 */
    int32_t dimension;        /**< 向量维度 */
    int32_t num_vectors;      /**< 当前向量数 */
    int32_t capacity;         /**< 容量上限 */
    int64_t created_at;       /**< 创建时间戳 */
    int64_t modified_at;      /**< 修改时间戳 */
    uint64_t lsn;             /**< 最后 LSN */

    /* 索引信息 */
    int32_t index_type;       /**< 索引类型 */
    void *index_data;         /**< 索引数据（序列化） */
    int32_t index_data_size;  /**< 索引数据大小 */

    /* 文件句柄 */
    void *file;               /**< 数据文件句柄 */
    void *meta_file;          /**< 元数据文件句柄 */

    /* 内部状态 */
    bool dirty;               /**< 是否有未刷盘的修改 */
    bool is_new;              /**< 是否新创建的段 */
} vector_segment_t;

/* ========================================================================
 * 向量段生命周期 API (A.3.1 - A.3.3)
 * ======================================================================== */

/**
 * @brief 创建向量段
 *
 * @param data_dir 数据目录
 * @param segment_id 段 ID
 * @param dimension 向量维度
 * @param capacity 容量上限
 * @return 段指针，失败返回 NULL
 */
vector_segment_t *vector_segment_create(const char *data_dir,
                                        int32_t segment_id,
                                        int32_t dimension,
                                        int32_t capacity);

/**
 * @brief 打开向量段
 *
 * @param data_dir 数据目录
 * @param segment_id 段 ID
 * @return 段指针，失败返回 NULL
 */
vector_segment_t *vector_segment_open(const char *data_dir, int32_t segment_id);

/**
 * @brief 关闭向量段
 *
 * @param seg 段指针
 * @param flush 是否刷盘
 */
void vector_segment_close(vector_segment_t *seg, bool flush);

/**
 * @brief 删除向量段
 *
 * @param seg 段指针
 * @return 0 成功，-1 失败
 */
int vector_segment_delete(vector_segment_t *seg);

/* ========================================================================
 * 向量操作 API (A.3.4 - A.3.5)
 * ======================================================================== */

/**
 * @brief 追加向量到段
 *
 * @param seg 段指针
 * @param vector 向量数据 [dimension]
 * @param vector_id 输出向量 ID（可为 NULL）
 * @return 0 成功，-1 失败
 */
int vector_segment_append(vector_segment_t *seg,
                          const float *vector,
                          int32_t *vector_id);

/**
 * @brief 批量追加向量
 *
 * @param seg 段指针
 * @param vectors 向量数组 [count * dimension]
 * @param count 向量数量
 * @param start_id 起始 ID（-1 表示自动分配）
 * @return 成功追加的数量，-1 表示失败
 */
int32_t vector_segment_append_batch(vector_segment_t *seg,
                                     const float *vectors,
                                     int32_t count,
                                     int32_t start_id);

/**
 * @brief 获取向量
 *
 * @param seg 段指针
 * @param vector_id 向量 ID
 * @param out_vector 输出缓冲区 [dimension]
 * @return 0 成功，-1 失败
 */
int vector_segment_get(const vector_segment_t *seg,
                       int32_t vector_id,
                       float *out_vector);

/**
 * @brief 获取向量数量
 *
 * @param seg 段指针
 * @return 向量数量
 */
int32_t vector_segment_count(const vector_segment_t *seg);

/* ========================================================================
 * 页面管理 API (A.2.1 - A.2.4)
 * ======================================================================== */

/**
 * @brief 分配新页面
 *
 * @param seg 段指针
 * @param is_index 是否为索引页
 * @return 页号，-1 表示失败
 */
int32_t vector_page_alloc(vector_segment_t *seg, bool is_index);

/**
 * @brief 写入页面
 *
 * @param seg 段指针
 * @param page_no 页号
 * @param data 数据
 * @param size 大小
 * @return 0 成功，-1 失败
 */
int vector_page_write(vector_segment_t *seg,
                      int32_t page_no,
                      const void *data,
                      size_t size);

/**
 * @brief 读取页面
 *
 * @param seg 段指针
 * @param page_no 页号
 * @param data 输出缓冲区
 * @param size 大小
 * @return 实际读取大小，-1 表示失败
 */
ssize_t vector_page_read(const vector_segment_t *seg,
                         int32_t page_no,
                         void *data,
                         size_t size);

/**
 * @brief 释放页面
 *
 * @param seg 段指针
 * @param page_no 页号
 * @return 0 成功，-1 失败
 */
int vector_page_free(vector_segment_t *seg, int32_t page_no);

/**
 * @brief 刷盘所有脏页
 *
 * @param seg 段指针
 * @return 0 成功，-1 失败
 */
int vector_segment_flush(vector_segment_t *seg);

/* ========================================================================
 * 索引持久化 API (A.5.1 - A.5.3)
 * ======================================================================== */

/**
 * @brief 保存索引数据
 *
 * @param seg 段指针
 * @param index_type 索引类型
 * @param index_data 索引数据
 * @param size 数据大小
 * @return 0 成功，-1 失败
 */
int vector_index_persist(vector_segment_t *seg,
                         vector_index_type_t index_type,
                         const void *index_data,
                         int32_t size);

/**
 * @brief 加载索引数据
 *
 * @param seg 段指针
 * @param index_type 索引类型
 * @param out_data 输出数据（调用者需释放）
 * @param out_size 输出大小
 * @return 0 成功，-1 失败
 */
int vector_index_load(const vector_segment_t *seg,
                      vector_index_type_t index_type,
                      void **out_data,
                      int32_t *out_size);

/* ========================================================================
 * WAL API (A.4.2 - A.4.4)
 * ======================================================================== */

/**
 * @brief 记录 WAL 日志
 *
 * @param seg 段指针
 * @param op_type 操作类型
 * @param data 日志数据
 * @param size 数据大小
 * @return 0 成功，-1 失败
 */
int vector_wal_log(vector_segment_t *seg,
                   vector_wal_op_type_t op_type,
                   const void *data,
                   size_t size);

/**
 * @brief 获取最后 LSN
 *
 * @param seg 段指针
 * @return 最后 LSN
 */
uint64_t vector_segment_get_lsn(const vector_segment_t *seg);

/**
 * @brief 回放 WAL 日志
 *
 * @param seg 段指针
 * @return 0 成功，-1 失败
 */
int vector_wal_replay(vector_segment_t *seg);

/* ========================================================================
 * 工具函数
 * ======================================================================== */

/**
 * @brief 获取段文件路径
 *
 * @param data_dir 数据目录
 * @param segment_id 段 ID
 * @param out_path 输出路径
 * @param path_size 路径缓冲区大小
 */
void vector_segment_get_path(const char *data_dir,
                             int32_t segment_id,
                             char *out_path,
                             size_t path_size);

/**
 * @brief 检查段是否存在
 *
 * @param data_dir 数据目录
 * @param segment_id 段 ID
 * @return true 存在，false 不存在
 */
bool vector_segment_exists(const char *data_dir, int32_t segment_id);

/**
 * @brief 获取段文件大小
 *
 * @param seg 段指针
 * @return 文件大小（字节）
 */
int64_t vector_segment_size(const vector_segment_t *seg);

/**
 * @brief 验证段完整性
 *
 * @param seg 段指针
 * @return true 完整，false 损坏
 */
bool vector_segment_verify(const vector_segment_t *seg);

/* ========================================================================
 * 段迭代器
 * ======================================================================== */

/**
 * @brief 段扫描回调
 *
 * @param vector_id 向量 ID
 * @param vector 向量数据
 * @param context 用户上下文
 * @return 0 继续，-1 停止
 */
typedef int (*vector_segment_scan_cb_t)(int32_t vector_id,
                                        const float *vector,
                                        void *context);

/**
 * @brief 扫描段内所有向量
 *
 * @param seg 段指针
 * @param callback 回调函数
 * @param context 用户上下文
 * @return 扫描的向量数
 */
int32_t vector_segment_scan(const vector_segment_t *seg,
                            vector_segment_scan_cb_t callback,
                            void *context);

#ifdef __cplusplus
}
#endif

#endif /* VECTOR_PERSIST_H */
