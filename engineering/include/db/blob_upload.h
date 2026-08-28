/**
 * @file blob_upload.h
 * @brief 统一流式 Upload Writer API（Task 5）
 *
 * 定义流式上传接口，支持任意大小数据的分块写入，
 * 自动管理 Chunk 分块、SHA-256 计算和两阶段发布。
 */
#ifndef DB_BLOB_UPLOAD_H
#define DB_BLOB_UPLOAD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 前向声明
 * ======================================================================== */

typedef struct blob_engine_s blob_engine_t;
typedef struct blob_upload_s blob_upload_t;

/* ========================================================================
 * 上传选项
 * ======================================================================== */

/**
 * @brief 上传选项
 */
typedef struct blob_upload_options_s {
    const char *upload_id;      /**< 上传会话标识（可选，NULL 则自动生成） */
    const char *content_type;   /**< 内容类型（可选） */
    const void *metadata;       /**< 元数据（可选） */
    size_t metadata_len;        /**< 元数据长度 */
} blob_upload_options_t;

/* ========================================================================
 * 返回码定义
 * ======================================================================== */

/** 操作成功 */
#define BLOB_UPLOAD_OK              0

/** 参数无效 */
#define BLOB_UPLOAD_ERR_INVAL       (-1)

/** I/O 错误 */
#define BLOB_UPLOAD_ERR_IO          (-2)

/** 状态转换无效 */
#define BLOB_UPLOAD_ERR_STATE       (-3)

/** 内存分配失败 */
#define BLOB_UPLOAD_ERR_NOMEM       (-4)

/* ========================================================================
 * Upload 生命周期 API
 * ======================================================================== */

/**
 * @brief 开始流式上传
 *
 * 创建上传会话，初始化 SHA-256 上下文和内部缓冲区。
 *
 * @param engine  Blob 引擎句柄
 * @param options 上传选项（可为 NULL）
 * @return 上传句柄，失败返回 NULL
 */
blob_upload_t *blob_upload_begin(blob_engine_t *engine,
                                 const blob_upload_options_t *options);

/**
 * @brief 写入上传数据
 *
 * 可多次调用，每次写入任意大小的数据。当内部缓冲区满时自动调用
 * blob_chunk_write_tmp() 发布 Chunk。零长度写入不产生 Chunk。
 *
 * @param upload 上传句柄
 * @param data   数据指针
 * @param len    数据长度（字节）
 * @return BLOB_UPLOAD_OK 成功，负值为错误码
 */
int blob_upload_write(blob_upload_t *upload, const void *data, size_t len);

/**
 * @brief 完成上传
 *
 * 执行两阶段发布协议：
 *   C0: 所有 Chunk 完成并 fsync
 *   C1: Catalog BLOB_PREPARE + WAL fsync
 *   C2: Manifest 临时写入 + fsync + rename
 *   C3: Catalog BLOB_COMMIT + WAL fsync
 *   C4: 内存状态 COMMITTED
 *   C5: 删除 upload 临时目录
 *
 * @param upload      上传句柄
 * @param out_blob_id 输出 Blob ID（32 字节 SHA-256）
 * @return BLOB_UPLOAD_OK 成功，负值为错误码
 */
int blob_upload_finish(blob_upload_t *upload,
                       uint8_t out_blob_id[32]);

/**
 * @brief 中止上传
 *
 * 删除上传会话的临时文件，不产生可见 Blob。
 *
 * @param upload 上传句柄
 * @return BLOB_UPLOAD_OK 成功，负值为错误码
 */
int blob_upload_abort(blob_upload_t *upload);

#ifdef __cplusplus
}
#endif

#endif /* DB_BLOB_UPLOAD_H */
