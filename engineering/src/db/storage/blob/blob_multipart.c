/**
 * @file blob_multipart.c
 * @brief Multipart 上传接口（包装新 Upload API）
 */
#include "db/blob_engine.h"
#include "db/blob_upload.h"
#include "db/blob_manifest.h"
#include "db/core/log.h"
#include <stdlib.h>
#include <string.h>

/* 内部结构：跟踪 Multipart 上传状态 */
typedef struct multipart_session_s {
    blob_engine_t *engine;
    blob_upload_t *upload;
    char upload_id[64];
    int *part_numbers;       /* 已上传的 part number 列表 */
    int part_count;
    int part_capacity;
} multipart_session_t;

/* 简化的 session 跟踪：使用静态变量存储当前 session */
static multipart_session_t *g_current_session = NULL;

int blob_multipart_begin(blob_engine_t *engine, const char *upload_id,
                          size_t total_size) {
    if (!engine || !upload_id) return -1;

    /* 创建 session */
    multipart_session_t *session = (multipart_session_t *)calloc(1, sizeof(multipart_session_t));
    if (!session) return -1;

    session->engine = engine;
    strncpy(session->upload_id, upload_id, sizeof(session->upload_id) - 1);

    /* 使用 upload API */
    blob_upload_options_t options = {0};
    options.upload_id = upload_id;
    session->upload = blob_upload_begin(engine, &options);
    if (!session->upload) {
        free(session);
        return -1;
    }

    /* 初始化 part 跟踪 */
    session->part_capacity = 16;
    session->part_numbers = (int *)calloc(session->part_capacity, sizeof(int));
    if (!session->part_numbers) {
        blob_upload_abort(session->upload);
        free(session);
        return -1;
    }

    /* 保存 session */
    g_current_session = session;

    return 0;
}

int blob_multipart_upload_part(blob_engine_t *engine, const char *upload_id,
                                int part_number, const void *data, size_t len) {
    (void)engine; (void)upload_id; (void)part_number; (void)data; (void)len;
    /* TODO: 实现 part number 校验和上传 */
    return 0;
}

int blob_multipart_complete(blob_engine_t *engine, const char *upload_id,
                             uint8_t out_blob_id[BLOB_SHA256_SIZE]) {
    (void)engine; (void)upload_id;
    memset(out_blob_id, 0, 32);
    /* TODO: 实现完成上传 */
    return 0;
}

int blob_multipart_abort(blob_engine_t *engine, const char *upload_id) {
    (void)engine;
    (void)upload_id;

    if (!g_current_session) {
        return 0;
    }

    /* 中止上传 */
    if (g_current_session->upload) {
        blob_upload_abort(g_current_session->upload);
    }

    /* 释放 part 列表 */
    free(g_current_session->part_numbers);

    /* 释放 session */
    free(g_current_session);
    g_current_session = NULL;

    return 0;
}
