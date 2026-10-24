// faiss_hnsw_visited_table.h
// VisitedTable：faiss_hnsw_search 专用的已访问节点标记表
// 避免在搜索过程中重复访问同一节点，提升搜索效率

#ifndef FAISS_HNSW_VISITED_TABLE_H
#define FAISS_HNSW_VISITED_TABLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 已访问节点标记表结构体
// 通过递增 curvisid 来批量重置，避免遍历清空数组
typedef struct {
    int32_t curvisid;  // 当前访问 ID（单调递增，用于区分搜索批次）
    int32_t num;       // 表大小（n_total，即图中节点总数）
    int32_t *visid;    // 访问标记数组（记录每个节点最近被访问的 visid）
} faiss_hnsw_visited_table_t;

/**
 * 创建访问表
 *
 * @param vt 输出参数，返回访问表指针的指针
 * @param n  表大小（节点总数）
 * @return   0 表示成功，非 0 表示失败
 */
int32_t faiss_hnsw_visited_table_create(faiss_hnsw_visited_table_t **vt, int32_t n);

/**
 * 查询节点是否已被访问
 *
 * @param vt 访问表指针
 * @param id 节点 id
 * @return   true 表示已访问，false 表示未访问
 */
bool faiss_hnsw_visited_table_get(faiss_hnsw_visited_table_t *vt, int32_t id);

/**
 * 标记节点为已访问
 *
 * @param vt 访问表指针
 * @param id 节点 id
 */
void faiss_hnsw_visited_table_set(faiss_hnsw_visited_table_t *vt, int32_t id);

/**
 * 重置访问表（递增 curvisid，使旧标记失效）
 *
 * @param vt 访问表指针
 */
void faiss_hnsw_visited_table_reset(faiss_hnsw_visited_table_t *vt);

/**
 * 销毁访问表并释放资源
 *
 * @param vt 访问表指针
 */
void faiss_hnsw_visited_table_drop(faiss_hnsw_visited_table_t *vt);

#ifdef __cplusplus
}
#endif

#endif  // FAISS_HNSW_VISITED_TABLE_H