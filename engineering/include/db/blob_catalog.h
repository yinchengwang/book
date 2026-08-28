/**
 * @file blob_catalog.h
 * @brief Blob 独立 Catalog 接口（Task 4）
 *
 * 定义 Blob 状态、内存索引、WAL 格式和 checkpoint 机制。
 * Catalog 不依赖 KV 事务，拥有独立的二进制 checkpoint 和 WAL。
 */
#ifndef DB_BLOB_CATALOG_H
#define DB_BLOB_CATALOG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

/** Catalog 目录名 */
#define BLOB_CATALOG_DIR         "catalog"

/** Checkpoint 文件名 */
#define BLOB_CATALOG_BIN         "catalog.bin"

/** WAL 文件名 */
#define BLOB_CATALOG_WAL         "catalog.wal"

/** 临时 checkpoint 文件名 */
#define BLOB_CATALOG_BIN_TMP     "catalog.bin.tmp"

/** Blob ID 长度（SHA-256） */
#define BLOB_CATALOG_ID_SIZE     32

/** Chunk ID 长度（SHA-256） */
#define BLOB_CATALOG_CHUNK_SIZE  32

/** GC 宽限期默认 1 小时（毫秒） */
#define BLOB_CATALOG_GC_GRACE_MS (60 * 60 * 1000LL)

/* ========================================================================
 * Blob 状态枚举
 * ======================================================================== */

/**
 * @brief Blob 条目状态
 */
typedef enum blob_entry_state_e {
    BLOB_STATE_PREPARED   = 1,  /**< 准备阶段：Chunk 已上传，等待 Commit */
    BLOB_STATE_COMMITTED  = 2,  /**< 已提交：Blob 对外可见 */
    BLOB_STATE_DELETED    = 3   /**< 已删除：标记删除，等待 GC */
} blob_entry_state_t;

/* ========================================================================
 * 数据结构定义
 * ======================================================================== */

/**
 * @brief Blob 条目（内存结构）
 */
typedef struct blob_entry_s {
    uint8_t              blob_id[BLOB_CATALOG_ID_SIZE];   /**< Blob ID（SHA-256） */
    blob_entry_state_t   state;                            /**< 当前状态 */
    uint64_t             blob_size;                        /**< Blob 大小（字节） */
    uint32_t             chunk_count;                      /**< Chunk 数量 */
    int64_t              created_at_ms;                    /**< 创建时间戳（毫秒） */
    int64_t              deleted_at_ms;                    /**< 删除时间戳（毫秒） */
} blob_entry_t;

/**
 * @brief Chunk 引用计数条目（内存结构）
 */
typedef struct blob_chunk_ref_s {
    uint8_t  chunk_id[BLOB_CATALOG_CHUNK_SIZE];  /**< Chunk ID（SHA-256） */
    uint64_t ref_count;                          /**< 引用计数 */
    int64_t  gc_after_ms;                        /**< 可 GC 时间（毫秒），0 表示不可 GC */
} blob_chunk_ref_t;

/**
 * @brief Catalog 句柄（不透明类型）
 */
typedef struct blob_catalog_s blob_catalog_t;

/* ========================================================================
 * 返回码定义
 * ======================================================================== */

/** 操作成功 */
#define BLOB_CATALOG_OK          0

/** 参数无效 */
#define BLOB_CATALOG_ERR_INVAL  (-1)

/** I/O 错误 */
#define BLOB_CATALOG_ERR_IO     (-2)

/** 记录不存在 */
#define BLOB_CATALOG_ERR_NOTFOUND (-3)

/** 校验失败 */
#define BLOB_CATALOG_ERR_CORRUPT (-4)

/** 内存分配失败 */
#define BLOB_CATALOG_ERR_NOMEM  (-5)

/** 状态转换无效 */
#define BLOB_CATALOG_ERR_STATE  (-6)

/* ========================================================================
 * WAL 记录类型
 * ======================================================================== */

/**
 * @brief WAL 记录类型枚举
 */
typedef enum blob_catalog_rec_type_e {
    /* Blob 生命周期记录 */
    BLOB_CATALOG_PREPARE   = 1,   /**< BLOB_PREPARE: 新建 Blob，状态=PREPARED */
    BLOB_CATALOG_COMMIT    = 2,   /**< BLOB_COMMIT: 提交 Blob，状态=COMMITTED */
    BLOB_CATALOG_DELETE    = 3,   /**< BLOB_DELETE: 标记删除，状态=DELETED */

    /* Chunk 引用计数记录 */
    BLOB_CATALOG_REF_INC   = 4,   /**< CHUNK_REF_INC: 引用计数 +1 */
    BLOB_CATALOG_REF_DEC   = 5,   /**< CHUNK_REF_DEC: 引用计数 -1 */

    /* Upload 会话记录 */
    BLOB_CATALOG_UPLOAD_BEGIN  = 6,  /**< UPLOAD_BEGIN: 开始上传会话 */
    BLOB_CATALOG_UPLOAD_ABORT  = 7   /**< UPLOAD_ABORT: 中止上传会话 */
} blob_catalog_rec_type_t;

/* ========================================================================
 * WAL 记录格式
 * ======================================================================== */

/**
 * @brief WAL 记录头（固定 16 字节）
 *
 * 完整记录布局：header(16) + payload(payload_len) + crc32(4)
 * 字段按 little-endian 写入。
 */
typedef struct blob_catalog_wal_header_s {
    uint32_t magic;             /**< 魔数：0x4341544C ('CATL') */
    uint16_t version;           /**< 格式版本：1 */
    uint16_t rec_type;          /**< 记录类型（blob_catalog_rec_type_e） */
    uint32_t payload_len;       /**< payload 字节长度 */
    uint32_t lsn;               /**< 日志序列号（递增） */
    uint32_t crc32;             /**< header + payload 的 CRC32 */
} blob_catalog_wal_header_t;

/** WAL 记录头固定大小 */
#define BLOB_CATALOG_WAL_HEADER_SIZE  16

/** WAL 魔数 */
#define BLOB_CATALOG_WAL_MAGIC   0x4341544CU

/** WAL 格式版本 */
#define BLOB_CATALOG_WAL_VERSION 1

/* ========================================================================
 * WAL Payload 结构（变长，按类型区分）
 * ======================================================================== */

/**
 * @brief BLOB_PREPARE / BLOB_COMMIT / BLOB_DELETE 记录的 payload
 */
typedef struct blob_catalog_blob_payload_s {
    uint8_t  blob_id[BLOB_CATALOG_ID_SIZE];
    uint64_t blob_size;
    uint32_t chunk_count;
} blob_catalog_blob_payload_t;

/**
 * @brief CHUNK_REF_INC / CHUNK_REF_DEC 记录的 payload
 */
typedef struct blob_catalog_chunk_payload_s {
    uint8_t chunk_id[BLOB_CATALOG_CHUNK_SIZE];
} blob_catalog_chunk_payload_t;

/**
 * @brief UPLOAD_BEGIN / UPLOAD_ABORT 记录的 payload
 */
typedef struct blob_catalog_upload_payload_s {
    uint8_t  upload_id[32];  /**< 上传会话 ID（UUID 等） */
} blob_catalog_upload_payload_t;

/* ========================================================================
 * Catalog 迭代器
 * ======================================================================== */

/**
 * @brief Catalog 扫描迭代器
 *
 * 用于遍历 Catalog 中的所有 Blob 条目。
 */
typedef struct blob_catalog_iter_s {
    void *opaque;  /**< 内部实现指针 */
} blob_catalog_iter_t;

/* ========================================================================
 * Catalog 操作 API
 * ======================================================================== */

/**
 * @brief 打开或创建 Catalog
 *
 * @param data_dir 数据根目录（Catalog 会创建 catalog/ 子目录）
 * @return Catalog 句柄，失败返回 NULL
 */
blob_catalog_t *blob_catalog_open(const char *data_dir);

/**
 * @brief 关闭 Catalog 并释放资源
 *
 * @param catalog Catalog 句柄
 */
void blob_catalog_close(blob_catalog_t *catalog);

/**
 * @brief 根据 blob_id 查找 Blob 条目
 *
 * @param catalog Catalog 句柄
 * @param blob_id Blob ID
 * @param out_entry 输出 Blob 条目（可为 NULL，仅用于存在性检查）
 * @return BLOB_CATALOG_OK 找到，BLOB_CATALOG_ERR_NOTFOUND 未找到
 */
int blob_catalog_find_blob(const blob_catalog_t *catalog,
                           const uint8_t blob_id[BLOB_CATALOG_ID_SIZE],
                           blob_entry_t *out_entry);

/**
 * @brief 根据 chunk_id 查找 Chunk 引用条目
 *
 * @param catalog Catalog 句柄
 * @param chunk_id Chunk ID
 * @param out_ref 输出 Chunk 引用条目（可为 NULL）
 * @return BLOB_CATALOG_OK 找到，BLOB_CATALOG_ERR_NOTFOUND 未找到
 */
int blob_catalog_find_chunk(const blob_catalog_t *catalog,
                            const uint8_t chunk_id[BLOB_CATALOG_CHUNK_SIZE],
                            blob_chunk_ref_t *out_ref);

/**
 * @brief 开始新事务（获取写锁）
 *
 * @param catalog Catalog 句柄
 * @return 0 成功，负值为错误码
 */
int blob_catalog_begin(blob_catalog_t *catalog);

/**
 * @brief 追加 Blob PREPARED 记录
 *
 * @param catalog Catalog 句柄
 * @param blob_id Blob ID
 * @param blob_size Blob 大小
 * @param chunk_count Chunk 数量
 * @return 0 成功，负值为错误码
 */
int blob_catalog_prepare(blob_catalog_t *catalog,
                         const uint8_t blob_id[BLOB_CATALOG_ID_SIZE],
                         uint64_t blob_size, uint32_t chunk_count);

/**
 * @brief 追加 Blob COMMITED 记录
 *
 * @param catalog Catalog 句柄
 * @param blob_id Blob ID
 * @return 0 成功，负值为错误码
 */
int blob_catalog_commit(blob_catalog_t *catalog,
                        const uint8_t blob_id[BLOB_CATALOG_ID_SIZE]);

/**
 * @brief 追加 Blob DELETED 记录
 *
 * @param catalog Catalog 句柄
 * @param blob_id Blob ID
 * @return 0 成功，负值为错误码
 */
int blob_catalog_delete(blob_catalog_t *catalog,
                        const uint8_t blob_id[BLOB_CATALOG_ID_SIZE]);

/**
 * @brief 追加 Chunk 引用计数 +1 记录
 *
 * @param catalog Catalog 句柄
 * @param chunk_id Chunk ID
 * @return 0 成功，负值为错误码
 */
int blob_catalog_ref_inc(blob_catalog_t *catalog,
                         const uint8_t chunk_id[BLOB_CATALOG_CHUNK_SIZE]);

/**
 * @brief 追加 Chunk 引用计数 -1 记录
 *
 * @param catalog Catalog 句柄
 * @param chunk_id Chunk ID
 * @return 0 成功，负值为错误码
 */
int blob_catalog_ref_dec(blob_catalog_t *catalog,
                         const uint8_t chunk_id[BLOB_CATALOG_CHUNK_SIZE]);

/**
 * @brief 提交事务（释放写锁，fsync WAL）
 *
 * @param catalog Catalog 句柄
 * @return 0 成功，负值为错误码
 */
int blob_catalog_end(blob_catalog_t *catalog);

/**
 * @brief 创建扫描迭代器
 *
 * @param catalog Catalog 句柄
 * @return 迭代器句柄，失败返回 NULL
 */
blob_catalog_iter_t *blob_catalog_iter_create(const blob_catalog_t *catalog);

/**
 * @brief 获取迭代器当前条目
 *
 * @param iter 迭代器
 * @param out_entry 输出 Blob 条目
 * @return BLOB_CATALOG_OK 成功，BLOB_CATALOG_ERR_NOTFOUND 遍历结束
 */
int blob_catalog_iter_next(blob_catalog_iter_t *iter, blob_entry_t *out_entry);

/**
 * @brief 销毁扫描迭代器
 *
 * @param iter 迭代器
 */
void blob_catalog_iter_destroy(blob_catalog_iter_t *iter);

/**
 * @brief 执行 checkpoint
 *
 * 将内存索引写入临时文件，fsync 后原子 rename。
 *
 * @param catalog Catalog 句柄
 * @return 0 成功，负值为错误码
 */
int blob_catalog_checkpoint(blob_catalog_t *catalog);

/**
 * @brief 加载 checkpoint 并重放 WAL（启动恢复）
 *
 * @param catalog Catalog 句柄
 * @return 0 成功，负值为错误码
 */
int blob_catalog_recover(blob_catalog_t *catalog);

/**
 * @brief 获取 Catalog 统计信息
 *
 * @param catalog Catalog 句柄
 * @param out_blob_count 输出 Blob 数量
 * @param out_chunk_count 输出 Chunk 引用数量
 * @return 0 成功
 */
int blob_catalog_stats(const blob_catalog_t *catalog,
                       uint64_t *out_blob_count,
                       uint64_t *out_chunk_count);

/**
 * @brief 获取 Catalog 数据目录
 *
 * @param catalog Catalog 句柄
 * @return 目录路径
 */
const char *blob_catalog_get_dir(const blob_catalog_t *catalog);

/**
 * @brief 获取当前 LSN
 *
 * @param catalog Catalog 句柄
 * @return 当前 LSN 值
 */
uint32_t blob_catalog_get_lsn(const blob_catalog_t *catalog);

#ifdef __cplusplus
}
#endif

#endif /* DB_BLOB_CATALOG_H */
