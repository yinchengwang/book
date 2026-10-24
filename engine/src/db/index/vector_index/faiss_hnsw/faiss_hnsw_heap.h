// faiss_hnsw_heap.h
// MinimaxHeap：faiss_hnsw_search 专用的大顶堆
// 与 FAISS HNSW::MinimaxHeap 语义完全一致
// 用于在 HNSW 图搜索过程中维护候选节点集合

#ifndef FAISS_HNSW_HEAP_H
#define FAISS_HNSW_HEAP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Minimax 堆结构体
// 固定容量 n 的数组实现的堆结构，nvalid 标识有效元素位置
typedef struct {
    int32_t n;      // 最大容量
    int32_t k;      // 当前元素数
    int32_t nvalid; // 有效元素数（指向 ids[0..nvalid-1] 区域）
    int32_t *ids;   // 节点 id 数组
    float *dis;     // 对应距离数组
} faiss_hnsw_minimax_heap_t;

/**
 * 创建 Minimax 堆
 *
 * @param heap  输出参数，返回堆指针的指针
 * @param n     最大容量
 * @return      0 表示成功，非 0 表示失败
 */
int32_t faiss_hnsw_minimax_heap_create(faiss_hnsw_minimax_heap_t **heap, int32_t n);

/**
 * 向堆中插入元素
 *
 * @param heap 堆指针
 * @param id   节点 id
 * @param dist 距离值
 */
void faiss_hnsw_minimax_heap_push(faiss_hnsw_minimax_heap_t *heap, int32_t id, float dist);

/**
 * 弹出最小元素
 *
 * @param heap     堆指针
 * @param dist_out 输出参数，返回弹出元素的距离
 * @return         弹出元素的 id，若堆为空返回 -1
 */
int32_t faiss_hnsw_minimax_heap_pop_min(faiss_hnsw_minimax_heap_t *heap, float *dist_out);

/**
 * 查看堆顶元素（最大距离）
 *
 * @param heap 堆指针
 * @return     堆顶元素距离
 */
float faiss_hnsw_minimax_heap_max(const faiss_hnsw_minimax_heap_t *heap);

/**
 * 获取堆中元素数量
 *
 * @param heap 堆指针
 * @return     当前元素数
 */
int32_t faiss_hnsw_minimax_heap_size(const faiss_hnsw_minimax_heap_t *heap);

/**
 * 清空堆
 *
 * @param heap 堆指针
 */
void faiss_hnsw_minimax_heap_clear(faiss_hnsw_minimax_heap_t *heap);

/**
 * 销毁堆并释放资源
 *
 * @param heap 堆指针
 */
void faiss_hnsw_minimax_heap_drop(faiss_hnsw_minimax_heap_t *heap);

#ifdef __cplusplus
}
#endif

#endif  // FAISS_HNSW_HEAP_H