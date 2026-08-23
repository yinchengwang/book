/**
 * @file graph.h
 * @brief 图模型内部接口
 */
#ifndef SDK_IMPL_GRAPH_H
#define SDK_IMPL_GRAPH_H

#include "sdk/mmdb.h"
#include "sdk/impl/mmdb_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 在 collection 首次写入时创建 nodes/edges 数据表 */
int mmdb_graph_ensure_tables(mmdb_collection_t* coll);

#ifdef __cplusplus
}
#endif

#endif /* SDK_IMPL_GRAPH_H */