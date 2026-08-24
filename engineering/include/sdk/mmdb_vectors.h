/**
 * @file mmdb_vectors.h
 * @brief 向量模型 API
 */
#ifndef SDK_MMDB_VECTORS_H
#define SDK_MMDB_VECTORS_H

#include "sdk/mmdb.h"

#ifdef __cplusplus
extern "C" {
#endif

int mmdb_vectors_add(mmdb_collection_t* c, const mmdb_vector_t* vecs, size_t n);
int mmdb_vectors_upsert(mmdb_collection_t* c, const mmdb_vector_t* vecs, size_t n);
int mmdb_vectors_search(mmdb_collection_t* c, const mmdb_query_t* q,
                        mmdb_result_t* out);
int mmdb_vectors_get(mmdb_collection_t* c, const uint8_t* id, size_t id_len,
                     mmdb_vector_t* out);
int mmdb_vectors_delete(mmdb_collection_t* c, const uint8_t* id, size_t id_len);

/**
 * @brief 为非 VECTOR 集合启用向量检索能力（P5-6）
 *
 * 用于 TEXT 等非 VECTOR 集合动态开启向量索引，使 hybrid_search 次通道
 * vector 在 TEXT 集合上真正激活。幂等：重复调用无副作用。
 *
 * @param c collection 句柄
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法
 */
int mmdb_vectors_enable(mmdb_collection_t* c);

#ifdef __cplusplus
}
#endif

#endif
