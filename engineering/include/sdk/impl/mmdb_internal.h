/**
 * @file mmdb_internal.h
 * @brief SDK 内部共享定义（仅供 src/sdk/ 下的 .c 文件使用，不对外暴露）
 */
#ifndef SDK_IMPL_MMDB_INTERNAL_H
#define SDK_IMPL_MMDB_INTERNAL_H

#include "sdk/mmdb.h"
#include "sdk/mmdb_types.h"
#include "sdk/mmdb_error.h"
#include "sdk/mmdb_embedding.h"  /* P4-T4.1：mmdb_collection_s 新增 embedding 字段 */

#include <pthread.h>
#include <stdint.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 不透明类型真实结构（仅实现可见） */
struct mmdb_s {
    sqlite3*                 db;             /* SQLite 后端句柄 */
    char*                   path;           /* 数据库文件路径 */
    mmdb_options_t          options;        /* 配置（拷贝） */
    int                     last_err;       /* 线程局部错误码副本（仅供非持有线程读） */
    char*                   last_err_msg;   /* 错误信息字符串 */
    pthread_rwlock_t         lock;           /* 写操作全局锁（支持并发读） */
    mmdb_collection_t**     collections;    /* 已打开的 collection 缓存 */
    size_t                  collection_count;
};

struct mmdb_collection_s {
    mmdb_t*                 db;             /* 归属数据库 */
    char*                   name;           /* Collection 名 */
    mmdb_model_t            model;          /* 模型类型 */
    mmdb_schema_t           schema;         /* Schema 拷贝（含 vector_dim） */
    sqlite3*                sdb;            /* SQLite 句柄（db->db 的别名，加速访问） */
    pthread_rwlock_t*        coll_lock;      /* 指向 db->lock（读写锁，支持并发读） */
    void*                   hnsw;           /* HNSW 索引指针（Phase 2，内存索引） */
    mmdb_embedding_t*       embedding;      /* P4-T4.1 新增：collection-level embedding；
                                            *   非 NULL 时 mmdb_rag_retrieve 默认用它。
                                            *   所有权归调用方，collection 不接管释放。 */
};

/* 错误信息缓冲最大长度（含末尾 \0） */
#define MMDB_ERR_MSG_MAX 512

/* 设置当前线程错误码与信息（内部使用） */
void mmdb_set_error(mmdb_t* db, int code, const char* msg);

/* 分配并复制字符串（失败时返回 NULL） */
char* mmdb_strdup_internal(const char* s);

/* 释放 mmdb_result_t 内部所有堆内存 */
void mmdb_result_release(mmdb_result_t* result);

/* 释放 mmdb_path_t 内部所有堆内存 */
void mmdb_path_release(mmdb_path_t* path);

#ifdef __cplusplus
}
#endif

#endif /* SDK_IMPL_MMDB_INTERNAL_H */