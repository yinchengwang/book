/**
 * @file datastore.h
 * @brief Yang datastore 三态（C2-5 T4）
 *
 * running / candidate / startup 三态 + candidate→running 原子提交
 */
#ifndef DB_DATASTORE_H
#define DB_DATASTORE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum datastore_type_e {
    DATASTORE_RUNNING = 0,
    DATASTORE_CANDIDATE = 1,
    DATASTORE_STARTUP = 2
} datastore_type_t;

typedef struct datastore_s {
    datastore_type_t type;
    char *xml_data;          /* 序列化的 XML 数据 */
    size_t xml_len;
    char *path;              /* 持久化路径 */
    void *wal;               /* C0-2 T6：可选 WAL 句柄（datastore commit 时记录） */
} datastore_t;

/* 加载 / 保存 / 提交 */
datastore_t *datastore_load(const char *path, datastore_type_t type);
int datastore_save(const datastore_t *ds);
int datastore_commit(datastore_t *running, datastore_t *candidate);  /* 原子 candidate→running */

/* C0-2 T6：设置 datastore 的 WAL 句柄（可选，NULL = 不记录） */
void datastore_set_wal(datastore_t *ds, void *wal);

void datastore_free(datastore_t *ds);

#ifdef __cplusplus
}
#endif

#endif /* DB_DATASTORE_H */