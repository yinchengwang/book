/**
 * @file blob_manifest.h
 * @brief Blob Chunk/Manifest 固定格式与原子发布接口（Task 2+3）
 *
 * 定义 Chunk 和 Manifest 文件的固定二进制格式，
 * 提供临时文件写入、原子发布（不覆盖式创建）和完整性校验读取。
 */
#ifndef DB_BLOB_MANIFEST_H
#define DB_BLOB_MANIFEST_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * Chunk 格式常量
 * ======================================================================== */

/** Chunk 文件魔数，'CHNK' 的十六进制 */
#define BLOB_CHUNK_MAGIC    0x43484E4BU

/** Chunk 格式版本 */
#define BLOB_CHUNK_VERSION  1

/** Chunk ID 长度（SHA-256 摘要） */
#define BLOB_CHUNK_ID_SIZE  32

/** 逻辑 Chunk 大小：4MB */
#define BLOB_CHUNK_LOGICAL_SIZE  (4U * 1024U * 1024U)

/* ========================================================================
 * Manifest 格式常量
 * ======================================================================== */

/** Manifest 文件魔数，'BLMF' 的十六进制 */
#define BLOB_MANIFEST_MAGIC     0x424C4D46U

/** Manifest 格式版本 */
#define BLOB_MANIFEST_VERSION   1

/** Manifest 头部固定大小（28 字节） */
#define BLOB_MANIFEST_HEADER_SIZE  28U

/** Manifest Chunk 条目固定大小（44 字节） */
#define BLOB_MANIFEST_CHUNK_SIZE   44U

/** Blob ID 长度（SHA-256 摘要） */
#define BLOB_BLOB_ID_SIZE      32

/* ========================================================================
 * Chunk 文件布局
 * ======================================================================== */

/**
 * @brief Chunk 文件头（固定 56 字节）
 *
 * 文件布局：header(56) + payload(payload_size) + payload_checksum(4)
 *
 * 所有多字节字段按 little-endian 写入。
 */
typedef struct blob_chunk_header_s {
    uint32_t magic;                    /**< 魔数，必须为 BLOB_CHUNK_MAGIC */
    uint32_t version;                  /**< 格式版本，必须为 BLOB_CHUNK_VERSION */
    uint64_t payload_size;             /**< 有效载荷字节数 */
    uint8_t  chunk_sha256[BLOB_CHUNK_ID_SIZE]; /**< payload 的 SHA-256 摘要 */
    uint32_t header_checksum;          /**< 除自身外前 48 字节的 CRC32 */
} blob_chunk_header_t;

/** Chunk 头部固定大小 */
#define BLOB_CHUNK_HEADER_SIZE  56U

/* ========================================================================
 * Manifest 文件布局
 * ======================================================================== */

/**
 * @brief Manifest 文件头（固定 28 字节）
 *
 * 文件布局：header(28) + content_type + metadata + chunks[]
 *
 * 所有多字节字段按 little-endian 写入。
 */
typedef struct blob_manifest_header_s {
    uint32_t magic;                    /**< 魔数，必须为 BLOB_MANIFEST_MAGIC */
    uint32_t version;                  /**< 格式版本，必须为 BLOB_MANIFEST_VERSION */
    uint32_t flags;                    /**< 保留标志位，当前为 0 */
    uint64_t blob_size;                /**< Blob 总字节数 */
    uint32_t chunk_size;               /**< 单个 Chunk 大小上限（字节） */
    uint32_t chunk_count;              /**< Chunk 条目数量 */
    uint16_t content_type_len;         /**< content_type 字符串长度（字节） */
    uint16_t metadata_len;             /**< metadata 字节长度 */
    uint8_t  blob_sha256[BLOB_BLOB_ID_SIZE]; /**< Blob 整体 SHA-256 摘要 */
    uint32_t manifest_checksum;        /**< 除自身外头部的 CRC32 */
} blob_manifest_header_t;

/**
 * @brief Manifest 中单个 Chunk 条目（固定 44 字节）
 */
typedef struct blob_manifest_chunk_s {
    uint8_t  chunk_sha256[BLOB_CHUNK_ID_SIZE]; /**< Chunk ID（SHA-256） */
    uint64_t logical_offset;           /**< 在 Blob 中的逻辑偏移 */
    uint32_t chunk_size;               /**< 该 Chunk 实际大小（字节） */
    uint32_t chunk_checksum;           /**< chunk_sha256 + logical_offset + chunk_size 的 CRC32 */
} blob_manifest_chunk_t;

/**
 * @brief Manifest 内存结构
 *
 * 包含 header、chunks 数组、content_type 和 metadata 的完整内存表示。
 */
typedef struct blob_manifest_s {
    blob_manifest_header_t header;     /**< Manifest 头部 */
    blob_manifest_chunk_t *chunks;      /**< Chunk 条目数组 */
    uint32_t chunk_count;               /**< Chunk 数量（与 header.chunk_count 一致） */
    char *content_type;                 /**< 内容类型字符串（可为 NULL） */
    void *metadata;                     /**< 元数据（可为 NULL） */
} blob_manifest_t;

/* ========================================================================
 * 返回码定义
 * ======================================================================== */

/** 操作成功 */
#define BLOB_OK             0

/** 参数无效 */
#define BLOB_ERR_INVAL     (-1)

/** I/O 错误（磁盘读写失败） */
#define BLOB_ERR_IO        (-2)

/** 文件不存在 */
#define BLOB_ERR_NOTFOUND  (-3)

/** 校验失败（magic/version/checksum/SHA-256 不匹配） */
#define BLOB_ERR_CORRUPT   (-4)

/** 内存分配失败 */
#define BLOB_ERR_NOMEM     (-5)

/** 正式文件已存在但内容不同，不允许覆盖 */
#define BLOB_ERR_CONFLICT  (-6)

/* ========================================================================
 * Chunk 写入与发布
 * ======================================================================== */

/**
 * @brief 将数据写入 Chunk 临时文件并原子发布
 *
 * 流程：
 *   1. 计算 SHA-256(data) 作为 chunk_id
 *   2. 创建临时文件 ".tmp.{upload_id}" 写入 header + payload + payload_checksum
 *   3. fflush + fsync 保证数据落盘
 *   4. 原子发布：若正式文件已存在，校验后复用；不同则返回错误
 *
 * @param dir          chunks 目录路径
 * @param data         待写入数据
 * @param len          数据长度（字节）
 * @param upload_id    上传会话标识，用于临时文件命名
 * @param out_chunk_id 输出 chunk ID（32 字节 SHA-256）
 * @return BLOB_OK 成功，负值为错误码
 */
int blob_chunk_write_tmp(const char *dir,
                         const void *data, size_t len,
                         const char *upload_id,
                         uint8_t out_chunk_id[BLOB_CHUNK_ID_SIZE]);

/**
 * @brief 读取并校验 Chunk 文件
 *
 * 依次校验：magic -> version -> header_checksum -> payload_size -> payload_checksum -> SHA-256
 * 任何校验失败返回 BLOB_ERR_CORRUPT 并清零 out_len。
 *
 * @param dir      chunks 目录路径
 * @param chunk_id chunk ID（32 字节 SHA-256）
 * @param out_buf  输出缓冲区（必须足够容纳 payload）
 * @param buf_len  输出缓冲区大小
 * @param out_len  实际读取的 payload 字节数
 * @return BLOB_OK 成功，负值为错误码
 */
int blob_chunk_read_checked(const char *dir,
                            const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE],
                            void *out_buf, size_t buf_len,
                            size_t *out_len);

/**
 * @brief 检查 Chunk 文件是否存在且可校验
 *
 * 验证文件存在且 header/payload/checksum 全部校验通过。
 *
 * @param dir      chunks 目录路径
 * @param chunk_id chunk ID
 * @return BLOB_OK 存在且可校验，BLOB_ERR_NOTFOUND 不存在，BLOB_ERR_CORRUPT 校验失败
 */
int blob_chunk_exists_checked(const char *dir,
                              const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE]);

/**
 * @brief 仅读取 Chunk header（不读 payload）
 *
 * @param dir        chunks 目录路径
 * @param chunk_id   chunk ID
 * @param out_header 输出 header 结构
 * @return BLOB_OK 成功，负值为错误码
 */
int blob_chunk_read_header(const char *dir,
                           const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE],
                           blob_chunk_header_t *out_header);

/**
 * @brief 获取 Chunk 临时文件路径
 *
 * 格式：{dir}/.tmp.{upload_id}
 *
 * @param dir      chunks 目录路径
 * @param upload_id 上传会话标识
 * @param path_buf 输出路径缓冲区
 * @param buf_size 缓冲区大小
 * @return BLOB_OK 成功
 */
int blob_chunk_tmp_path(const char *dir, const char *upload_id,
                        char *path_buf, size_t buf_size);

/**
 * @brief 获取 Chunk 正式文件路径
 *
 * 格式：{dir}/{chunk_id_hex}.chunk
 *
 * @param dir      chunks 目录路径
 * @param chunk_id chunk ID
 * @param path_buf 输出路径缓冲区
 * @param buf_size 缓冲区大小
 * @return BLOB_OK 成功
 */
int blob_chunk_final_path(const char *dir,
                          const uint8_t chunk_id[BLOB_CHUNK_ID_SIZE],
                          char *path_buf, size_t buf_size);

/**
 * @brief 计算 header_checksum（覆盖 magic ~ chunk_sha256，不含 checksum 自身）
 *
 * CRC32 覆盖前 48 字节：magic(4) + version(4) + payload_size(8) + chunk_sha256(32)
 */
uint32_t blob_chunk_header_checksum(const blob_chunk_header_t *hdr);

/**
 * @brief 计算 payload 的 CRC32
 */
uint32_t blob_chunk_payload_checksum(const void *data, size_t len);

/* ========================================================================
 * Manifest 内存结构管理
 * ======================================================================== */

/**
 * @brief 创建 Manifest 内存结构
 *
 * @param chunk_count Chunk 数量
 * @param content_type 内容类型字符串（可为 NULL）
 * @param metadata 元数据（可为 NULL）
 * @param metadata_len 元数据长度
 * @return 分配的 Manifest 结构，失败返回 NULL
 */
blob_manifest_t *blob_manifest_create(uint32_t chunk_count,
                                      const char *content_type,
                                      const void *metadata, size_t metadata_len);

/**
 * @brief 释放 Manifest 内存结构
 *
 * @param manifest 要释放的 Manifest
 */
void blob_manifest_free(blob_manifest_t *manifest);

/* ========================================================================
 * Manifest 写入与发布
 * ======================================================================== */

/**
 * @brief 原子写入 Manifest 文件
 *
 * 使用临时文件写入，完成后 fsync 并 rename 为正式文件。
 * 临时文件格式：.tmp.{upload_id}
 *
 * @param dir        manifests 目录路径
 * @param manifest   Manifest 内存结构
 * @param upload_id  上传会话标识
 * @return BLOB_OK 成功，负值为错误码
 */
int blob_manifest_write_atomic(const char *dir,
                               const blob_manifest_t *manifest,
                               const char *upload_id);

/**
 * @brief 读取并校验 Manifest 文件
 *
 * 依次校验：magic -> version -> manifest_checksum -> blob_sha256
 *
 * @param dir       manifests 目录路径
 * @param blob_id   Blob ID（用于定位 manifest 文件）
 * @param out_manifest 输出 Manifest 结构（调用者负责释放）
 * @return BLOB_OK 成功，负值为错误码
 */
int blob_manifest_load_checked(const char *dir,
                               const uint8_t blob_id[BLOB_BLOB_ID_SIZE],
                               blob_manifest_t **out_manifest);

/**
 * @brief 获取 Manifest 正式文件路径
 *
 * 格式：{dir}/{blob_id_hex}.manifest
 *
 * @param dir       manifests 目录路径
 * @param blob_id   Blob ID
 * @param path_buf  输出路径缓冲区
 * @param buf_size  缓冲区大小
 * @return BLOB_OK 成功
 */
int blob_manifest_final_path(const char *dir,
                              const uint8_t blob_id[BLOB_BLOB_ID_SIZE],
                              char *path_buf, size_t buf_size);

/**
 * @brief 获取 Manifest 临时文件路径
 *
 * 格式：{dir}/.tmp.{upload_id}
 *
 * @param dir        manifests 目录路径
 * @param upload_id  上传会话标识
 * @param path_buf   输出路径缓冲区
 * @param buf_size   缓冲区大小
 * @return BLOB_OK 成功
 */
int blob_manifest_tmp_path(const char *dir, const char *upload_id,
                           char *path_buf, size_t buf_size);

/**
 * @brief 计算 Manifest 头部的 CRC32
 *
 * 覆盖前 24 字节：magic + version + flags + blob_size + chunk_size +
 *                 chunk_count + content_type_len + metadata_len + blob_sha256
 */
uint32_t blob_manifest_header_checksum(const blob_manifest_header_t *hdr);

/**
 * @brief 计算 Manifest Chunk 条目的 CRC32
 *
 * 覆盖前 40 字节：chunk_sha256(32) + logical_offset(8)
 */
uint32_t blob_manifest_chunk_checksum(const blob_manifest_chunk_t *chunk);

/* ========================================================================
 * SHA-256 转十六进制工具函数
 * ======================================================================== */

/**
 * @brief SHA-256 摘要转十六进制字符串
 *
 * @param digest     SHA-256 摘要（32 字节）
 * @param hex        输出缓冲区（至少 65 字节）
 */
void blob_sha256_to_hex(const uint8_t digest[BLOB_BLOB_ID_SIZE],
                        char hex[BLOB_BLOB_ID_SIZE * 2 + 1]);

#ifdef __cplusplus
}
#endif

#endif /* DB_BLOB_MANIFEST_H */
