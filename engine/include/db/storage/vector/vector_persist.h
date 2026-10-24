/**
 * @file vector_persist.h
 * @brief 向量索引持久化层公共接口
 *
 * ============================================================================
 * 设计目标
 * ============================================================================
 *
 * 将向量索引数据持久化到磁盘，实现崩溃恢复能力。
 *
 * 架构设计：
 * ```
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    向量持久化层架构                              │
 * ├─────────────────────────────────────────────────────────────────┤
 * │                                                                 │
 * │   应用层                                                         │
 * │     │                                                           │
 * │     ▼                                                           │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              VectorSegment (向量段)                      │  │
 * │   │   - 管理向量数据的追加/读取                              │  │
 * │   │   - 封装多个 VectorPage                                 │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │                                                           │
 * │     ▼                                                           │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              VectorPage (向量页)                         │  │
 * │   │   - 固定大小页面 (8KB)                                   │  │
 * │   │   - 存储批量向量数据                                     │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │                                                           │
 * │     ▼                                                           │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              Buffer Pool (页面缓存)                      │  │
 * │   │   - 缓存热点页面                                         │  │
 * │   │   - 自动淘汰                                             │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │                                                           │
 * │     ▼                                                           │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              WAL (预写日志)                              │  │
 * │   │   - 保证数据不丢失                                       │  │
 * │   │   - 支持崩溃恢复                                         │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │     │                                                           │
 * │     ▼                                                           │
 * │   ┌─────────────────────────────────────────────────────────┐  │
 * │   │              磁盘文件                                    │  │
 * │   │   - segment_XXX.dat                                     │  │
 * │   │   - segment_XXX.idx                                     │  │
 * │   └─────────────────────────────────────────────────────────┘  │
 * │                                                                 │
 * └─────────────────────────────────────────────────────────────────┘
 * ```
 *
 * ============================================================================
 * 文件格式
 * ============================================================================
 *
 * segment_XXX.dat (数据文件)：
 * ┌─────────────────────────────────────────────────────────┐
 * │ Header (64 bytes)                                       │
 * │  - magic: 0x56454354 ("VECT")  [4 bytes]               │
 * │  - version: 1                   [4 bytes]              │
 * │  - segment_id: 段ID              [4 bytes]             │
 * │  - dims: 向量维度                [4 bytes]             │
 * │  - page_size: 页面大小           [4 bytes]             │
 * │  - vector_size: 单向量大小       [4 bytes]             │
 * │  - num_vectors: 向量数量         [8 bytes]             │
 * │  - num_pages: 页面数量           [4 bytes]             │
 * │  - reserved                      [28 bytes]            │
 * ├─────────────────────────────────────────────────────────┤
 * │ Pages (8192 bytes each)                                 │
 * │  ┌─────────────────────────────────────────────────┐    │
 * │  │ PageHeader (16 bytes)                           │    │
 * │  │  - page_id: 页面ID        [4 bytes]             │    │
 * │  │  - num_vectors: 向量数    [4 bytes]             │    │
 * │  │  - free_space: 空闲空间   [4 bytes]             │    │
 * │  │  - checksum: 校验和       [4 bytes]             │    │
 * │  ├─────────────────────────────────────────────────┤    │
 * │  │ VectorData (8176 bytes)                         │    │
 * │  │  - 批量向量数据                                  │    │
 * │  └─────────────────────────────────────────────────┘    │
 * └─────────────────────────────────────────────────────────┘
 *
 * segment_XXX.idx (索引文件)：
 * ┌─────────────────────────────────────────────────────────┐
 * │ Header (64 bytes)                                       │
 * ├─────────────────────────────────────────────────────────┤
 * │ Index Entries (每个 16 字节)                            │
 * │  - vector_id: 向量ID           [4 bytes]               │
 * │  - page_id: 所在页面ID         [4 bytes]               │
 * │  - offset: 页内偏移            [4 bytes]               │
 * │  - reserved                    [4 bytes]               │
 * └─────────────────────────────────────────────────────────┘
 *
 * ============================================================================
 * 使用示例
 * ============================================================================
 *
 * @code
 * // 1. 创建向量段
 * vector_segment_t *seg = vector_segment_create("vectors", 128, 0);
 *
 * // 2. 追加向量
 * float vec[128] = {...};
 * int32_t id = vector_segment_append(seg, vec);
 *
 * // 3. 获取向量
 * float retrieved[128];
 * vector_segment_get(seg, id, retrieved);
 *
 * // 4. 关闭段
 * vector_segment_close(seg);
 * @endcode
 */
#ifndef DB_STORAGE_VECTOR_PERSIST_H
#define DB_STORAGE_VECTOR_PERSIST_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 常量定义
 * ============================================================ */

/** 向量段文件魔数 "VECT" */
#define VECTOR_SEGMENT_MAGIC 0x56454354

/** 向量段版本号 */
#define VECTOR_SEGMENT_VERSION 1

/** 向量段文件头大小 */
#define VECTOR_SEGMENT_HEADER_SIZE 64

/** 向量页文件头大小 */
#define VECTOR_PAGE_HEADER_SIZE 16

/** 向量页无效偏移值（表示追加失败） */
#define VECTOR_PAGE_INVALID_OFFSET UINT16_MAX

/** 默认向量段 ID */
#define VECTOR_SEGMENT_DEFAULT_ID 0

/** 向量页中单个向量元数据大小 */
#define VECTOR_INDEX_ENTRY_SIZE 16

/* ============================================================
 * 页面类型
 * ============================================================ */

/** 向量页面类型 */
typedef enum vector_page_type_e {
    VECTOR_PAGE_DATA = 0,    /**< 数据页 */
    VECTOR_PAGE_INDEX = 1    /**< 索引页 (ID 映射) */
} vector_page_type_t;

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct vector_segment_s vector_segment_t;
typedef struct vector_persist_page_s vector_persist_page_t;
typedef struct vector_index_entry_s vector_index_entry_t;

/* ============================================================
 * 向量页头部 (完整定义，前置声明后可访问)
 * ============================================================ */

/**
 * @brief 向量页头部结构（持久化层专用）
 */
#pragma pack(push, 1)
typedef struct vector_persist_page_header_s {
    uint32_t page_id;        /**< 页面ID */
    uint16_t num_vectors;    /**< 页内向量数 */
    uint16_t free_space;     /**< 空闲空间 */
    uint32_t checksum;       /**< 校验和 */
    uint8_t  reserved[4];    /**< 保留字段 */
} vector_persist_page_header_t;
#pragma pack(pop)

/**
 * @brief 向量页内部结构（持久化层专用）
 */
struct vector_persist_page_s {
    vector_persist_page_header_t header;  /**< 页面头部 */
    uint8_t             *data;    /**< 页面数据区 */
    uint32_t             page_size; /**< 页面大小 */
    bool                 is_owner;  /**< 是否拥有数据区内存 */
};

/**
 * @brief 向量索引条目结构
 *
 * 用于在索引页中快速定位向量数据。
 */
#pragma pack(push, 1)
typedef struct vector_index_entry_s {
    int32_t  vector_id;      /**< 向量ID */
    uint32_t page_id;        /**< 所在页面ID */
    uint16_t offset;         /**< 页内偏移 */
    uint16_t reserved;       /**< 保留字段 */
} vector_index_entry_t;
#pragma pack(pop)

/* ============================================================
 * 向量段头部
 * ============================================================ */

/**
 * @brief 向量段头部结构
 */
#pragma pack(push, 1)
typedef struct vector_segment_header_s {
    uint32_t magic;          /**< 魔数 0x56454354 */
    uint32_t version;        /**< 版本号 */
    uint32_t segment_id;     /**< 段ID */
    int32_t  dims;           /**< 向量维度 */
    uint32_t page_size;      /**< 页面大小 */
    uint32_t vector_size;    /**< 单向量大小 (bytes) */
    uint64_t num_vectors;    /**< 向量数量 */
    uint32_t num_pages;      /**< 页面数量 */
    uint32_t num_deleted;    /**< 已删除向量数 */
    uint64_t created_at;     /**< 创建时间戳 */
    uint64_t modified_at;    /**< 修改时间戳 */
    uint8_t  reserved[8];    /**< 保留字段 */
} vector_segment_header_t;
#pragma pack(pop)

/* ============================================================
 * 向量段状态
 * ============================================================ */

/** 向量段状态 */
typedef enum vector_segment_state_e {
    VECTOR_SEGMENT_CLOSED = 0,   /**< 关闭状态 */
    VECTOR_SEGMENT_OPEN = 1,     /**< 打开状态 */
    VECTOR_SEGMENT_READONLY = 2  /**< 只读状态 */
} vector_segment_state_t;

/* ============================================================
 * 向量段 API
 * ============================================================ */

/**
 * @brief 创建新的向量段
 * @param path 段文件所在目录
 * @param dims 向量维度
 * @param segment_id 段ID
 * @return 向量段指针，失败返回 NULL
 */
vector_segment_t *vector_segment_create(const char *path, int32_t dims, uint32_t segment_id);

/**
 * @brief 打开已有向量段
 * @param path 段文件所在目录
 * @param segment_id 段ID
 * @return 向量段指针，失败返回 NULL
 */
vector_segment_t *vector_segment_open(const char *path, uint32_t segment_id);

/**
 * @brief 关闭向量段
 * @param seg 向量段指针
 * @return 0 成功
 */
int vector_segment_close(vector_segment_t *seg);

/**
 * @brief 删除向量段
 * @param path 段文件所在目录
 * @param segment_id 段ID
 * @return 0 成功
 */
int vector_segment_drop(const char *path, uint32_t segment_id);

/**
 * @brief 追加向量到段
 * @param seg 向量段指针
 * @param vector 向量数据
 * @return 分配的向量ID，失败返回 -1
 */
int32_t vector_segment_append(vector_segment_t *seg, const float *vector);

/**
 * @brief 批量追加向量
 * @param seg 向量段指针
 * @param vectors 向量数据数组
 * @param n 向量数量
 * @return 起始向量ID，失败返回 -1
 */
int32_t vector_segment_append_batch(vector_segment_t *seg, const float *vectors, int32_t n);

/**
 * @brief 获取向量
 * @param seg 向量段指针
 * @param vector_id 向量ID
 * @param out_vector 输出缓冲区
 * @return 0 成功，-1 失败
 */
int vector_segment_get(const vector_segment_t *seg, int32_t vector_id, float *out_vector);

/**
 * @brief 批量获取向量
 * @param seg 向量段指针
 * @param vector_ids 向量ID数组
 * @param n ID数量
 * @param out_vectors 输出缓冲区
 * @return 成功获取的数量
 */
int32_t vector_segment_get_batch(const vector_segment_t *seg, const int32_t *vector_ids,
                                  int32_t n, float *out_vectors);

/**
 * @brief 删除向量（标记删除）
 * @param seg 向量段指针
 * @param vector_id 向量ID
 * @return 0 成功，-1 失败
 */
int vector_segment_delete(vector_segment_t *seg, int32_t vector_id);

/**
 * @brief 获取向量段中的向量数量
 * @param seg 向量段指针
 * @return 向量数量
 */
int64_t vector_segment_size(const vector_segment_t *seg);

/**
 * @brief 获取向量维度
 * @param seg 向量段指针
 * @return 向量维度
 */
int32_t vector_segment_dims(const vector_segment_t *seg);

/**
 * @brief 获取段ID
 * @param seg 向量段指针
 * @return 段ID
 */
uint32_t vector_segment_id(const vector_segment_t *seg);

/**
 * @brief 获取向量段状态
 * @param seg 向量段指针
 * @return 状态
 */
vector_segment_state_t vector_segment_state(const vector_segment_t *seg);

/**
 * @brief 刷新向量段（刷脏页）
 * @param seg 向量段指针
 * @return 0 成功
 */
int vector_segment_flush(vector_segment_t *seg);

/* ============================================================
 * 向量页 API (内部使用)
 * ============================================================ */

/**
 * @brief 创建向量页
 * @param page_id 页面ID
 * @param page_size 页面大小
 * @return 向量页指针
 */
vector_persist_page_t *vector_persist_page_create(uint32_t page_id, uint32_t page_size);

/**
 * @brief 从数据初始化向量页
 * @param data 页面数据
 * @param size 数据大小
 * @return 向量页指针
 */
vector_persist_page_t *vector_persist_page_from_data(void *data, size_t size);

/**
 * @brief 释放向量页
 * @param page 向量页指针
 */
void vector_persist_page_free(vector_persist_page_t *page);

/**
 * @brief 获取页面头部
 * @param page 向量页指针
 * @return 页面头部指针
 */
vector_persist_page_header_t *vector_persist_page_header(vector_persist_page_t *page);

/**
 * @brief 获取页面数据区
 * @param page 向量页指针
 * @return 数据区指针
 */
void *vector_persist_page_data(vector_persist_page_t *page);

/**
 * @brief 获取页面可用空间
 * @param page 向量页指针
 * @return 可用空间字节数
 */
uint16_t vector_persist_page_free_space(const vector_persist_page_t *page);

/**
 * @brief 向页面追加向量
 * @param page 向量页指针
 * @param vector 向量数据
 * @param dims 向量维度
 * @return 分配的偏移，失败返回 0
 */
uint16_t vector_persist_page_append(vector_persist_page_t *page, const float *vector, int32_t dims);

/**
 * @brief 从页面读取向量
 * @param page 向量页指针
 * @param offset 偏移
 * @param dims 向量维度
 * @param out_vector 输出缓冲区
 */
void vector_persist_page_read(const vector_persist_page_t *page, uint16_t offset, int32_t dims, float *out_vector);

/**
 * @brief 验证页面校验和
 * @param page 向量页指针
 * @return 校验通过返回 true
 */
bool vector_persist_page_verify_checksum(const vector_persist_page_t *page);

/**
 * @brief 计算并设置页面校验和
 * @param page 向量页指针
 */
void vector_persist_page_set_checksum(vector_persist_page_t *page);

/* ============================================================
 * WAL 集成 API
 * ============================================================ */

/** 向量操作类型 */
typedef enum vector_wal_op_e {
    VECTOR_WAL_APPEND = 1,      /**< 追加向量 */
    VECTOR_WAL_DELETE = 2,      /**< 删除向量 */
    VECTOR_WAL_SEGMENT_CREATE = 3, /**< 创建段 */
    VECTOR_WAL_SEGMENT_DROP = 4  /**< 删除段 */
} vector_wal_op_t;

/**
 * @brief 向量 WAL 记录头
 */
#pragma pack(push, 1)
typedef struct vector_wal_header_s {
    uint8_t  op;            /**< 操作类型 */
    uint32_t segment_id;    /**< 段ID */
    int32_t  vector_id;     /**< 向量ID (-1 表示无效) */
    int32_t  dims;          /**< 向量维度 */
    uint32_t data_size;     /**< 数据大小 */
} vector_wal_header_t;
#pragma pack(pop)

/**
 * @brief 序列化向量 WAL 记录
 * @param op 操作类型
 * @param segment_id 段ID
 * @param vector_id 向量ID
 * @param dims 向量维度
 * @param data 向量数据
 * @param out_buf 输出缓冲区
 * @param buf_size 缓冲区大小
 * @return 序列化的字节数，失败返回 0
 */
size_t vector_wal_serialize(vector_wal_op_t op, uint32_t segment_id,
                            int32_t vector_id, int32_t dims,
                            const float *data, void *out_buf, size_t buf_size);

/**
 * @brief 反序列化向量 WAL 记录
 * @param buf 输入缓冲区
 * @param buf_size 缓冲区大小
 * @param out_op 输出操作类型
 * @param out_segment_id 输出段ID
 * @param out_vector_id 输出向量ID
 * @param out_dims 输出向量维度
 * @param out_data 输出向量数据
 * @return 0 成功，-1 失败
 */
int vector_wal_deserialize(const void *buf, size_t buf_size,
                           vector_wal_op_t *out_op, uint32_t *out_segment_id,
                           int32_t *out_vector_id, int32_t *out_dims,
                           float *out_data);

/* ============================================================
 * WAL 配置参数
 * ============================================================ */

/** WAL 文件魔数 "VWAL" */
#define VECTOR_WAL_MAGIC 0x5657414C

/** WAL 版本号 */
#define VECTOR_WAL_VERSION 1

/** WAL 文件头大小 */
#define VECTOR_WAL_HEADER_SIZE 128

/** 默认 WAL 缓冲区大小 */
#define VECTOR_WAL_BUFFER_SIZE (64 * 1024)

/** 默认 checkpoint 间隔（秒） */
#define VECTOR_WAL_CHECKPOINT_INTERVAL 300

/** WAL 记录头大小 */
#define VECTOR_WAL_RECORD_HEADER_SIZE 17

/**
 * @brief WAL 模式
 */
typedef enum vector_wal_mode_e {
    VECTOR_WAL_SYNC = 0,     /**< 同步模式（每次写入都刷盘） */
    VECTOR_WAL_ASYNC = 1,    /**< 异步模式（后台线程刷盘） */
    VECTOR_WAL_NONE = 2      /**< 关闭 WAL */
} vector_wal_mode_t;

/**
 * @brief Checkpoint 类型
 */
typedef enum vector_checkpoint_type_e {
    VECTOR_CHECKPOINT_SHUTDOWN = 0,   /**< 关闭检查点 */
    VECTOR_CHECKPOINT_MANUAL = 1,     /**< 手动检查点 */
    VECTOR_CHECKPOINT_TIMED = 2       /**< 定时检查点 */
} vector_checkpoint_type_t;

/**
 * @brief WAL 文件头部
 */
#pragma pack(push, 1)
typedef struct vector_wal_file_header_s {
    uint32_t magic;              /**< 魔数 0x5657414C */
    uint32_t version;            /**< 版本号 */
    uint64_t created_at;         /**< 创建时间戳 */
    uint64_t last_checkpoint;    /**< 上次检查点位置 */
    uint64_t segment_id;         /**< 关联的段ID */
    uint64_t num_records;        /**< 总记录数 */
    uint64_t num_vectors;        /**< 总向量数 */
    uint8_t  mode;               /**< WAL 模式 */
    uint8_t  reserved[39];       /**< 保留字段 */
} vector_wal_file_header_t;
#pragma pack(pop)

/* ============================================================
 * WAL 上下文
 * ============================================================ */

/**
 * @brief 向量 WAL 上下文
 */
typedef struct vector_wal_s {
    char    path[256];           /**< WAL 文件路径 */
    FILE   *fp;                  /**< WAL 文件指针 */
    uint8_t *buffer;             /**< 写缓冲区 */
    size_t  buffer_size;         /**< 缓冲区大小 */
    size_t  buffer_used;         /**< 缓冲区已用大小 */
    uint64_t num_records;        /**< 记录计数 */
    uint64_t num_vectors;        /**< 向量计数 */
    uint64_t last_checkpoint;    /**< 上次检查点位置 */
    vector_wal_mode_t mode;      /**< WAL 模式 */
    bool    is_open;             /**< 是否打开 */
    pthread_mutex_t mutex;       /**< 互斥锁 */
} vector_wal_t;

/**
 * @brief WAL 记录信息
 */
typedef struct vector_wal_record_info_s {
    vector_wal_op_t op;          /**< 操作类型 */
    uint32_t segment_id;         /**< 段ID */
    int32_t  vector_id;          /**< 向量ID */
    uint64_t offset;             /**< 文件偏移 */
    uint32_t size;               /**< 记录大小 */
} vector_wal_record_info_t;

/* ============================================================
 * WAL API
 * ============================================================ */

/**
 * @brief 创建 WAL
 * @param path WAL 文件目录
 * @param segment_id 关联的段ID
 * @return WAL 指针，失败返回 NULL
 */
vector_wal_t *vector_wal_create(const char *path, uint32_t segment_id);

/**
 * @brief 打开已有 WAL
 * @param path WAL 文件目录
 * @param segment_id 关联的段ID
 * @return WAL 指针，失败返回 NULL
 */
vector_wal_t *vector_wal_open(const char *path, uint32_t segment_id);

/**
 * @brief 关闭 WAL
 * @param wal WAL 指针
 * @return 0 成功
 */
int vector_wal_close(vector_wal_t *wal);

/**
 * @brief 删除 WAL
 * @param path WAL 文件目录
 * @param segment_id 段ID
 * @return 0 成功
 */
int vector_wal_drop(const char *path, uint32_t segment_id);

/**
 * @brief 写入追加向量记录
 * @param wal WAL 指针
 * @param segment_id 段ID
 * @param vector_id 向量ID
 * @param dims 向量维度
 * @param vector 向量数据
 * @return 0 成功，-1 失败
 */
int vector_wal_append(vector_wal_t *wal, uint32_t segment_id,
                      int32_t vector_id, int32_t dims, const float *vector);

/**
 * @brief 写入删除向量记录
 * @param wal WAL 指针
 * @param segment_id 段ID
 * @param vector_id 向量ID
 * @return 0 成功，-1 失败
 */
int vector_wal_delete(vector_wal_t *wal, uint32_t segment_id, int32_t vector_id);

/**
 * @brief 刷新 WAL 缓冲区
 * @param wal WAL 指针
 * @return 0 成功
 */
int vector_wal_flush(vector_wal_t *wal);

/**
 * @brief 执行检查点
 * @param wal WAL 指针
 * @param type 检查点类型
 * @return 0 成功，-1 失败
 */
int vector_wal_checkpoint(vector_wal_t *wal, vector_checkpoint_type_t type);

/**
 * @brief 截断 WAL 到检查点
 * @param wal WAL 指针
 * @return 0 成功，-1 失败
 */
int vector_wal_truncate(vector_wal_t *wal);

/**
 * @brief 获取 WAL 记录数
 * @param wal WAL 指针
 * @return 记录数
 */
uint64_t vector_wal_num_records(const vector_wal_t *wal);

/**
 * @brief 获取 WAL 文件大小
 * @param wal WAL 指针
 * @return 文件大小
 */
uint64_t vector_wal_file_size(const vector_wal_t *wal);

/**
 * @brief 重放 WAL 到向量段
 * @param wal_path WAL 文件目录
 * @param segment_id 段ID
 * @param segment 向量段指针
 * @param callback 进度回调 (progress, total, user_data)，返回 true 继续
 * @param user_data 用户数据
 * @return 恢复的记录数，-1 失败
 */
int64_t vector_wal_replay(const char *wal_path, uint32_t segment_id,
                          vector_segment_t *segment,
                          bool (*callback)(int64_t progress, int64_t total, void *user_data),
                          void *user_data);

/* ============================================================
 * 崩溃恢复 API
 * ============================================================ */

/** 恢复状态 */
typedef enum vector_recovery_status_e {
    VECTOR_RECOVERY_OK = 0,           /**< 恢复成功 */
    VECTOR_RECOVERY_NO_WAL = 1,       /**< 无 WAL 文件 */
    VECTOR_RECOVERY_CORRUPTED = 2,    /**< WAL 损坏 */
    VECTOR_RECOVERY_FAILED = 3        /**< 恢复失败 */
} vector_recovery_status_t;

/** 恢复结果信息 */
typedef struct vector_recovery_result_s {
    vector_recovery_status_t status;  /**< 恢复状态 */
    uint64_t records_recovered;       /**< 恢复的记录数 */
    uint64_t vectors_recovered;       /**< 恢复的向量数 */
    uint64_t elapsed_ms;              /**< 恢复耗时(毫秒) */
    char error_msg[256];              /**< 错误信息 */
} vector_recovery_result_t;

/**
 * @brief 恢复向量索引
 * @param data_dir 数据目录
 * @param segment_id 段ID
 * @return 恢复结果，失败返回 NULL
 */
vector_recovery_result_t *vector_index_recover(const char *data_dir, uint32_t segment_id);

/**
 * @brief 获取检查点信息
 * @param wal_path WAL 目录
 * @param segment_id 段ID
 * @param out_checkpoint 输出检查点位置
 * @param out_records 输出记录数
 * @return 0 成功，-1 失败
 */
int vector_wal_get_checkpoint_info(const char *wal_path, uint32_t segment_id,
                                   uint64_t *out_checkpoint, uint64_t *out_records);

/**
 * @brief 释放恢复结果
 * @param result 恢复结果指针
 */
void vector_recovery_result_free(vector_recovery_result_t *result);

/**
 * @brief 获取恢复状态描述
 * @param status 恢复状态
 * @return 状态描述字符串
 */
const char *vector_recovery_status_str(vector_recovery_status_t status);

/**
 * @brief 打印恢复报告
 * @param result 恢复结果指针
 */
void vector_recovery_print_report(const vector_recovery_result_t *result);

/* ============================================================
 * 索引持久化 API
 * ============================================================ */

/** 索引类型 (用于持久化) */
typedef enum vector_persist_index_type_e {
    VECTOR_PERSIST_INDEX_HNSW = 0,     /**< HNSW 索引 */
    VECTOR_PERSIST_INDEX_DISKANN = 1,  /**< DiskANN 索引 */
    VECTOR_PERSIST_INDEX_IVF = 2       /**< IVF 索引 */
} vector_persist_index_type_t;

/**
 * @brief 保存 HNSW 索引结构
 * @param hnsw_index HNSW 索引指针
 * @param path 输出文件路径
 * @return 0 成功，-1 失败
 */
int vector_index_save_hnsw(void *hnsw_index, const char *path);

/**
 * @brief 加载 HNSW 索引结构
 * @param path 输入文件路径
 * @return HNSW 索引指针，失败返回 NULL
 */
void *vector_index_load_hnsw(const char *path);

/**
 * @brief 保存 DiskANN 索引结构
 * @param diskann_index DiskANN 索引指针
 * @param path 输出文件路径
 * @return 0 成功，-1 失败
 */
int vector_index_save_diskann(void *diskann_index, const char *path);

/**
 * @brief 加载 DiskANN 索引结构
 * @param path 输入文件路径
 * @return DiskANN 索引指针，失败返回 NULL
 */
void *vector_index_load_diskann(const char *path);

/**
 * @brief 保存 IVF 索引结构
 * @param ivf_index IVF 索引指针
 * @param path 输出文件路径
 * @return 0 成功，-1 失败
 */
int vector_index_save_ivf(void *ivf_index, const char *path);

/**
 * @brief 加载 IVF 索引结构
 * @param path 输入文件路径
 * @return IVF 索引指针，失败返回 NULL
 */
void *vector_index_load_ivf(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* DB_STORAGE_VECTOR_PERSIST_H */
