/**
 * @file blob_manifest.h
 * @brief Blob Chunk 固定格式与原子发布接口（Task 2）
 *
 * 定义 Chunk 文件的固定二进制格式（56 字节头 + payload + payload_checksum），
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

#ifdef __cplusplus
}
#endif

#endif /* DB_BLOB_MANIFEST_H */
