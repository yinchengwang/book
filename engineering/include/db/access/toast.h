/**
 * @file toast.h
 * @brief TOAST（The Oversized-Attribute Storage Technique）接口
 *
 * TOAST 用于存储超过页面大小的元组数据：
 * 1. 当元组 > 2KB 时，先用 pglz 压缩
 * 2. 压缩后仍 > 2KB，则外部存储到 pg_toast_<oid> 表
 * 3. 主元组只存储指向外部数据的指针
 *
 * 参考 PostgreSQL: src/backend/access/heap/toast_store.c
 */
#ifndef DB_ACCESS_TOAST_H
#define DB_ACCESS_TOAST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * TOAST 常量
 * ============================================================ */

/** Toast 阈值（元组超过此大小触发 TOAST） */
#define TOAST_TUPLE_THRESHOLD    2000

/** Toast 最大内联大小（超过此大小外部存储） */
#define TOAST_MAX_INLINE_SIZE   2000

/** Toast 页面大小（8KB） */
#define TOAST_PAGE_SIZE          8192

/** Toast 块大小（每次读取的块大小） */
#define TOAST_CHUNK_SIZE        1996

/** Toast 压缩后最大大小 */
#define TOAST_MAX_COMPRESSED    (TOAST_PAGE_SIZE - 128)

/** Toast 目标压缩率 */
#define TOAST_TARGET_COMPRESS_RATE  0.7

/* ============================================================
 * Toast 压缩方法
 * ============================================================ */

/** Toast 压缩方法 */
typedef enum ToastMethod {
    TOAST_METHOD_NONE = 0,      /**< 不压缩 */
    TOAST_METHOD_PGLZ = 1,    /**< PGLZ 压缩 */
    TOAST_METHOD_EXTERNAL = 2  /**< 外部存储（不压缩） */
} ToastMethod;

/** Toast 压缩信息 */
typedef struct ToastCompressed {
    uint8_t  method;            /**< 压缩方法 */
    uint8_t  flags;            /**< 标志 */
    uint16_t original_size;     /**< 原始大小 */
    uint16_t compressed_size;   /**< 压缩后大小 */
    uint8_t  data[1];          /**< 压缩数据（变长） */
} ToastCompressed;

/* ============================================================
 * Toast 状态码
 * ============================================================ */

/** Toast 操作结果 */
#define TOAST_SUCCESS           0
#define TOAST_ERROR_TOO_LARGE   (-1)
#define TOAST_ERROR_COMPRESS    (-2)
#define TOAST_ERROR_IO          (-3)
#define TOAST_ERROR_NO_SPACE    (-4)

/* ============================================================
 * Toast API
 * ============================================================ */

/**
 * @brief 初始化 TOAST 子系统
 * @return 0 成功
 */
int toast_init(void);

/**
 * @brief 关闭 TOAST 子系统
 */
void toast_shutdown(void);

/**
 * @brief 压缩数据
 * @param src 源数据
 * @param src_len 源数据长度
 * @param dst 输出缓冲区
 * @param dst_len 输出缓冲区大小
 * @return 压缩后大小，失败返回 -1
 */
int toast_compress(const void *src, size_t src_len,
                   void *dst, size_t dst_len);

/**
 * @brief 解压缩数据
 * @param src 压缩数据
 * @param src_len 压缩数据长度
 * @param dst 输出缓冲区
 * @param dst_len 输出缓冲区大小
 * @return 解压后大小，失败返回 -1
 */
int toast_decompress(const void *src, size_t src_len,
                    void *dst, size_t dst_len);

/**
 * @brief 存储大值到 Toast 表
 * @param relOid 表的 OID
 * @param value 值数据
 * @param value_len 值长度
 * @param chunk_id 输出：Chunk ID
 * @return 0 成功
 */
int toast_store(uint32_t relOid, const void *value, size_t value_len,
                uint32_t *chunk_id);

/**
 * @brief 从 Toast 表读取值
 * @param relOid 表的 OID
 * @param chunk_id Chunk ID
 * @param buffer 输出缓冲区
 * @param buffer_len 缓冲区大小
 * @return 读取的字节数，失败返回 -1
 */
int toast_fetch(uint32_t relOid, uint32_t chunk_id,
                void *buffer, size_t buffer_len);

/**
 * @brief 检查数据是否需要 TOAST 处理
 * @param size 数据大小
 * @return true 需要 TOAST
 */
static inline bool toast_needs_toast(size_t size) {
    return size > TOAST_TUPLE_THRESHOLD;
}

/**
 * @brief 获取压缩后的大小估算
 * @param original_size 原始大小
 * @return 压缩后大小估算
 */
static inline size_t toast_estimate_compressed_size(size_t original_size) {
    /* 压缩后通常比原始大小小 */
    return original_size + 64;  /* 加上头部开销 */
}

#ifdef __cplusplus
}
#endif

#endif /* DB_ACCESS_TOAST_H */
