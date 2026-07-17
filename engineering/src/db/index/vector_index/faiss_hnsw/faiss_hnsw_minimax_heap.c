// faiss_hnsw_minimax_heap.c
// MinimaxHeap 实现：参考 FAISS HNSW::MinimaxHeap 重写为纯 C 版本
//
// 关键设计：
//   - 内部使用 CMax 类型的最大堆（dis[0] 为堆顶 = 当前最大距离）
//   - 容量为 n（ef），但 nvalid 表示有效元素数（未 -1 标记的）
//   - push：堆满时若新元素更大则丢弃；否则弹出最大再插入新元素
//   - pop_min：从有效区域线性扫描找到最小距离的 id 并返回

#include "db/index/vector_index/faiss_hnsw/faiss_hnsw_heap.h"

#include <math.h>
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// 创建 Minimax 堆
int32_t faiss_hnsw_minimax_heap_create(faiss_hnsw_minimax_heap_t **heap, int32_t n) {
    if (!heap || n <= 0) {
        return -1;
    }

    faiss_hnsw_minimax_heap_t *h = (faiss_hnsw_minimax_heap_t *)calloc(1, sizeof(faiss_hnsw_minimax_heap_t));
    if (!h) {
        return -1;
    }

    h->n = n;
    h->k = 0;
    h->nvalid = 0;
    h->ids = (int32_t *)malloc((size_t)n * sizeof(int32_t));
    h->dis = (float *)malloc((size_t)n * sizeof(float));
    if (!h->ids || !h->dis) {
        free(h->ids);
        free(h->dis);
        free(h);
        return -1;
    }

    // 初始化为 -1 标记，FAISS 中 pop_min 通过检查 ids[j] < 0 跳过
    for (int i = 0; i < n; i++) {
        h->ids[i] = -1;
        h->dis[i] = 0.0f;
    }

    *heap = h;
    return 0;
}

// 销毁堆
void faiss_hnsw_minimax_heap_drop(faiss_hnsw_minimax_heap_t *heap) {
    if (!heap) {
        return;
    }
    free(heap->ids);
    free(heap->dis);
    free(heap);
}

// 堆上滤：CMax 语义（最大堆），子节点 > 父节点时交换
static void sift_up(faiss_hnsw_minimax_heap_t *heap, int32_t pos) {
    while (pos > 0) {
        int32_t parent = (pos - 1) / 2;
        if (heap->dis[pos] > heap->dis[parent]) {
            // 交换
            float tmp_d = heap->dis[pos];
            int32_t tmp_i = heap->ids[pos];
            heap->dis[pos] = heap->dis[parent];
            heap->ids[pos] = heap->ids[parent];
            heap->dis[parent] = tmp_d;
            heap->ids[parent] = tmp_i;
            pos = parent;
        } else {
            break;
        }
    }
}

// 堆下滤：CMax 语义
static void sift_down(faiss_hnsw_minimax_heap_t *heap, int32_t pos) {
    int32_t k = heap->k;
    while (true) {
        int32_t left = 2 * pos + 1;
        int32_t right = 2 * pos + 2;
        int32_t largest = pos;

        if (left < k && heap->dis[left] > heap->dis[largest]) {
            largest = left;
        }
        if (right < k && heap->dis[right] > heap->dis[largest]) {
            largest = right;
        }
        if (largest == pos) {
            break;
        }

        float tmp_d = heap->dis[pos];
        int32_t tmp_i = heap->ids[pos];
        heap->dis[pos] = heap->dis[largest];
        heap->ids[pos] = heap->ids[largest];
        heap->dis[largest] = tmp_d;
        heap->ids[largest] = tmp_i;
        pos = largest;
    }
}

// 弹出堆顶（最大元素）
static void heap_pop_max(faiss_hnsw_minimax_heap_t *heap) {
    if (heap->k <= 0) {
        return;
    }
    heap->k--;
    if (heap->k > 0) {
        heap->dis[0] = heap->dis[heap->k];
        heap->ids[0] = heap->ids[heap->k];
        heap->ids[heap->k] = -1;
        sift_down(heap, 0);
    } else {
        heap->ids[0] = -1;
    }
}

// push：与 FAISS HNSW::MinimaxHeap::push 行为完全一致
void faiss_hnsw_minimax_heap_push(faiss_hnsw_minimax_heap_t *heap, int32_t id, float dist) {
    if (!heap) {
        return;
    }

    if (heap->k == heap->n) {
        // 堆已满
        if (dist >= heap->dis[0]) {
            // 新元素比当前最大还大（或等于），直接丢弃
            return;
        }
        // 否则弹出最大元素
        if (heap->ids[0] != -1) {
            heap->nvalid--;
        }
        heap_pop_max(heap);
    }

    // 在末尾插入新元素，然后上滤
    heap->ids[heap->k] = id;
    heap->dis[heap->k] = dist;
    heap->k++;
    heap->nvalid++;
    sift_up(heap, heap->k - 1);
}

// max：返回堆顶元素（最大距离）
float faiss_hnsw_minimax_heap_max(const faiss_hnsw_minimax_heap_t *heap) {
    if (!heap || heap->k <= 0) {
        return FLT_MAX;
    }
    return heap->dis[0];
}

// size：返回有效元素数（nvalid）
int32_t faiss_hnsw_minimax_heap_size(const faiss_hnsw_minimax_heap_t *heap) {
    if (!heap) {
        return 0;
    }
    return heap->nvalid;
}

// pop_min：从有效区域（ids[0..n-1] 中 ids[i] >= 0 的）线性扫描找最小距离
// 这是与 FAISS HNSW::MinimaxHeap::pop_min 兼容的简化实现
int32_t faiss_hnsw_minimax_heap_pop_min(faiss_hnsw_minimax_heap_t *heap, float *dist_out) {
    if (!heap || heap->k <= 0) {
        if (dist_out) {
            *dist_out = FLT_MAX;
        }
        return -1;
    }

    // 线性扫描找最小
    int32_t min_idx = -1;
    float min_dist = FLT_MAX;
    for (int32_t i = 0; i < heap->k; i++) {
        if (heap->ids[i] >= 0 && heap->dis[i] < min_dist) {
            min_dist = heap->dis[i];
            min_idx = i;
        }
    }

    if (min_idx < 0) {
        // 没有有效元素
        if (dist_out) {
            *dist_out = FLT_MAX;
        }
        return -1;
    }

    int32_t result_id = heap->ids[min_idx];

    // 从堆中删除该元素（与堆尾交换后下滤，保持堆结构）
    int32_t last = heap->k - 1;
    if (min_idx != last) {
        heap->dis[min_idx] = heap->dis[last];
        heap->ids[min_idx] = heap->ids[last];
    }
    heap->ids[last] = -1;
    heap->k--;
    heap->nvalid--;

    if (min_idx < heap->k) {
        // 调整堆：先尝试上滤，再下滤（或者直接 sift_down）
        sift_down(heap, min_idx);
        sift_up(heap, min_idx);
    }

    if (dist_out) {
        *dist_out = min_dist;
    }
    return result_id;
}

// clear：清空堆
void faiss_hnsw_minimax_heap_clear(faiss_hnsw_minimax_heap_t *heap) {
    if (!heap) {
        return;
    }
    heap->k = 0;
    heap->nvalid = 0;
    for (int i = 0; i < heap->n; i++) {
        heap->ids[i] = -1;
    }
}
