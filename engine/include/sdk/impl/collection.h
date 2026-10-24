/**
 * @file collection.h
 * @brief Collection 内部接口（CRUD）
 */
#ifndef SDK_IMPL_COLLECTION_H
#define SDK_IMPL_COLLECTION_H

#include "sdk/mmdb.h"
#include "sdk/impl/mmdb_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 Collection 注册表（首次 open 调用） */
int mmdb_collection_init(mmdb_t* db);

/* 加载所有已存在的 collection 到 db->collections（bootstrap） */
int mmdb_collection_load_all(mmdb_t* db);

/* 关闭时释放 db->collections */
void mmdb_collection_dispose(mmdb_t* db);

/* 按名查找（内部使用） */
mmdb_collection_t* mmdb_collection_find(mmdb_t* db, const char* name);

/* 插入一条 collection 元数据 */
int mmdb_collection_insert_meta(mmdb_t* db, const char* name,
                                mmdb_model_t model, const char* schema_json,
                                size_t vector_dim);

/* 删除一条 collection 元数据 */
int mmdb_collection_delete_meta(mmdb_t* db, const char* name);

#ifdef __cplusplus
}
#endif

#endif /* SDK_IMPL_COLLECTION_H */