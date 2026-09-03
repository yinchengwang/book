/**
 * @file object_storage.h
 * @brief 对象存储接口
 *
 * Phase12 - 实现对象存储，追赶 MinIO/S3 水平。
 */
#ifndef DB_STORAGE_OBJECT_H
#define DB_STORAGE_OBJECT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 对象存储不透明类型 */
typedef struct object_store object_store_t;

/** 对象信息 */
typedef struct {
    char key[1024];
    size_t size;
    uint64_t created_time;
    char etag[64];
} object_info_t;

/** 创建对象存储 */
object_store_t *object_store_create(const char *data_dir, size_t default_size);

/** 关闭存储 */
void object_store_close(object_store_t *store);

/** 上传对象 */
int object_put(object_store_t *store, const char *key, const void *data, size_t size);

/** 下载对象 */
void *object_get(object_store_t *store, const char *key, size_t *out_size);

/** 删除对象 */
int object_delete(object_store_t *store, const char *key);

/** 检查对象是否存在 */
bool object_exists(object_store_t *store, const char *key);

/** 列出对象 */
object_info_t *object_list(object_store_t *store, const char *prefix, size_t *out_count);

/** 释放对象列表 */
void object_list_free(object_info_t *list, size_t count);

/** 获取对象大小 */
size_t object_size(object_store_t *store, const char *key);

#ifdef __cplusplus
}
#endif
#endif /* DB_STORAGE_OBJECT_H */
