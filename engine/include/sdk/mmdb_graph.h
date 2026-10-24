/**
 * @file mmdb_graph.h
 * @brief 图模型 API
 */
#ifndef SDK_MMDB_GRAPH_H
#define SDK_MMDB_GRAPH_H

#include "sdk/mmdb.h"

#ifdef __cplusplus
extern "C" {
#endif

int mmdb_graph_add_node(mmdb_collection_t* c, const mmdb_node_t* node);
int mmdb_graph_add_edge(mmdb_collection_t* c, const mmdb_edge_t* edge);
int mmdb_graph_delete_node(mmdb_collection_t* c, const char* node_id);
int mmdb_graph_delete_edge(mmdb_collection_t* c, const char* source_id,
                            const char* target_id, const char* edge_label);
int mmdb_graph_shortest_path(mmdb_collection_t* c, const char* from_id,
                              const char* to_id, mmdb_path_t* out);
int mmdb_graph_bfs(mmdb_collection_t* c, const char* start_id, size_t max_depth,
                    mmdb_result_t* out);
int mmdb_graph_dfs(mmdb_collection_t* c, const char* start_id, size_t max_depth,
                    mmdb_result_t* out);

#ifdef __cplusplus
}
#endif

#endif
