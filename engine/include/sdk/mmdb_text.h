/**
 * @file mmdb_text.h
 * @brief 文本模型 API
 */
#ifndef SDK_MMDB_TEXT_H
#define SDK_MMDB_TEXT_H

#include "sdk/mmdb.h"

#ifdef __cplusplus
extern "C" {
#endif

int mmdb_text_add(mmdb_collection_t* c, const mmdb_text_entry_t* entry);
int mmdb_text_add_batch(mmdb_collection_t* c, const mmdb_text_entry_t* entries,
                         size_t n);
int mmdb_text_search(mmdb_collection_t* c, const mmdb_text_query_t* q,
                      mmdb_result_t* out);
int mmdb_text_get(mmdb_collection_t* c, const char* id,
                   mmdb_text_entry_t* out);
int mmdb_text_delete(mmdb_collection_t* c, const char* id);

/**
 * @brief 为非 TEXT 集合启用文本检索能力（P5-6）
 *
 * 用于 VECTOR 等非 TEXT 集合动态开启 FTS5 文本检索能力，使 hybrid_search
 * 次通道 text 在 VECTOR 集合上真正激活。幂等：重复调用无副作用。
 *
 * @param c collection 句柄
 * @return MMDB_OK 成功；MMDB_ERR_INVALID 参数非法
 */
int mmdb_text_enable(mmdb_collection_t* c);

#ifdef __cplusplus
}
#endif

#endif
