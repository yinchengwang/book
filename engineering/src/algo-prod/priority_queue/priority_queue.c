#include "algo-prod/priority_queue/priority_queue.h"

#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * 内部常量
 * ────────────────────────────────────────────────────────────────────────── */

#define PQ_DEFAULT_CAPACITY  16u
#define PQ_GROWTH_FACTOR     2u

/*
 * 交换临时缓冲区阈值（字节）。
 * 小于等于此大小时使用栈上缓冲区，避免 malloc 开销；
 * 覆盖 int/float/double/指针及绝大多数小值类型。
 */
#define PQ_SWAP_BUF_SIZE     256u

/* ──────────────────────────────────────────────────────────────────────────
 * 内部辅助：堆索引计算
 * ────────────────────────────────────────────────────────────────────────── */

static size_t pq_parent(size_t i)
{
    return (i - 1u) / 2u;
}

static size_t pq_left(size_t i)
{
    return 2u * i + 1u;
}

static size_t pq_right(size_t i)
{
    return 2u * i + 2u;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 内部辅助：元素访问
 * ────────────────────────────────────────────────────────────────────────── */

static char *pq_elem(pq_t *pq, size_t i)
{
    return (char *)pq->data + i * pq->element_size;
}

static const char *pq_elem_c(const pq_t *pq, size_t i)
{
    return (const char *)pq->data + i * pq->element_size;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 内部辅助：元素交换
 * 优先使用栈缓冲区，大元素降级为 malloc 临时缓冲区。
 * ────────────────────────────────────────────────────────────────────────── */

static void pq_swap(pq_t *pq, size_t i, size_t j)
{
    char   stack_buf[PQ_SWAP_BUF_SIZE];
    char  *heap_buf = NULL;
    char  *tmp;
    char  *a;
    char  *b;

    if (i == j) {
        return;
    }

    a = pq_elem(pq, i);
    b = pq_elem(pq, j);

    if (pq->element_size <= PQ_SWAP_BUF_SIZE) {
        tmp = stack_buf;
    } else {
        heap_buf = (char *)malloc(pq->element_size);
        if (!heap_buf) {
            return;
        }
        tmp = heap_buf;
    }

    memcpy(tmp, a, pq->element_size);
    memcpy(a,   b, pq->element_size);
    memcpy(b, tmp, pq->element_size);

    free(heap_buf); /* NULL 时 free 为无操作 */
}

/* ──────────────────────────────────────────────────────────────────────────
 * 内部辅助：堆维护
 *
 * 对应 C++ priority_queue 底层 push_heap / pop_heap 调整过程。
 * sift_up   — 上浮，用于 push 后恢复堆序（O(log n)）
 * sift_down — 下沉，用于 pop 后或 heapify 中恢复堆序（O(log n)）
 * ────────────────────────────────────────────────────────────────────────── */

static void pq_sift_up(pq_t *pq, size_t i)
{
    while (i > 0u) {
        size_t p = pq_parent(i);

        if (pq->compare(pq_elem_c(pq, i), pq_elem_c(pq, p)) > 0) {
            pq_swap(pq, i, p);
            i = p;
        } else {
            break;
        }
    }
}

static void pq_sift_down(pq_t *pq, size_t i)
{
    for (;;) {
        size_t best = i;
        size_t lc   = pq_left(i);
        size_t rc   = pq_right(i);

        if (lc < pq->size && pq->compare(pq_elem_c(pq, lc), pq_elem_c(pq, best)) > 0) {
            best = lc;
        }
        if (rc < pq->size && pq->compare(pq_elem_c(pq, rc), pq_elem_c(pq, best)) > 0) {
            best = rc;
        }
        if (best == i) {
            break;
        }
        pq_swap(pq, i, best);
        i = best;
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * 内部辅助：扩容
 * 使用 realloc 原地扩展，避免手动 malloc + memcpy + free。
 * ────────────────────────────────────────────────────────────────────────── */

static int pq_grow(pq_t *pq)
{
    size_t  new_capacity;
    void   *new_data;

    new_capacity = pq->capacity * PQ_GROWTH_FACTOR;
    if (new_capacity == pq->capacity) {
        new_capacity = pq->capacity + 1u;
    }

    new_data = realloc(pq->data, new_capacity * pq->element_size);
    if (!new_data) {
        return -1;
    }

    pq->data     = new_data;
    pq->capacity = new_capacity;
    return 0;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 创建 / 销毁
 * ────────────────────────────────────────────────────────────────────────── */

pq_t *pq_create(size_t element_size,
                           int (*compare)(const void *a, const void *b))
{
    return pq_create_ex(element_size, 0u, compare, NULL);
}

pq_t *pq_create_ex(size_t element_size,
                              size_t initial_capacity,
                              int (*compare)(const void *a, const void *b),
                              void (*free_element)(void *element))
{
    pq_t *pq;

    if (element_size == 0u || !compare) {
        return NULL;
    }
    if (initial_capacity == 0u) {
        initial_capacity = PQ_DEFAULT_CAPACITY;
    }

    pq = (pq_t *)malloc(sizeof(pq_t));
    if (!pq) {
        return NULL;
    }

    pq->data = malloc(initial_capacity * element_size);
    if (!pq->data) {
        free(pq);
        return NULL;
    }

    pq->size          = 0u;
    pq->capacity      = initial_capacity;
    pq->element_size  = element_size;
    pq->compare       = compare;
    pq->free_element  = free_element;
    return pq;
}

/*
 * Floyd 建堆算法（O(n)）
 *
 * 原理：叶节点已是合法单节点堆，从最后一个非叶节点（(n-2)/2）起
 * 向前依次执行 sift_down，令每棵子树满足堆性质，整体复杂度 O(n)。
 * 相比逐个 push 的 O(n log n) 更高效，适合已有数据批量入堆场景。
 */
pq_t *pq_heapify(const void *array,
                            size_t count,
                            size_t element_size,
                            int (*compare)(const void *a, const void *b))
{
    pq_t *pq;
    size_t     i;

    if (!array || element_size == 0u || !compare) {
        return NULL;
    }

    pq = pq_create_ex(element_size, count > 0u ? count : 1u, compare, NULL);
    if (!pq) {
        return NULL;
    }

    if (count == 0u) {
        return pq;
    }

    memcpy(pq->data, array, count * element_size);
    pq->size = count;

    /* 从最后一个非叶节点向根逐步下沉 */
    if (count > 1u) {
        i = (count - 2u) / 2u;
        for (;;) {
            pq_sift_down(pq, i);
            if (i == 0u) {
                break;
            }
            i -= 1u;
        }
    }

    return pq;
}

void pq_destroy(pq_t *pq)
{
    if (!pq) {
        return;
    }
    if (pq->free_element) {
        size_t i;

        for (i = 0u; i < pq->size; i++) {
            pq->free_element(pq_elem(pq, i));
        }
    }
    free(pq->data);
    free(pq);
}

/* ──────────────────────────────────────────────────────────────────────────
 * 核心操作
 * ────────────────────────────────────────────────────────────────────────── */

/*
 * push：拷贝 element 至堆末尾，sift_up 恢复堆序。O(log n)
 * 对应 C++ std::priority_queue::push。
 */
int pq_push(pq_t *pq, const void *element)
{
    if (!pq || !element) {
        return -1;
    }

    if (pq->size >= pq->capacity) {
        if (pq_grow(pq) != 0) {
            return -1;
        }
    }

    memcpy(pq_elem(pq, pq->size), element, pq->element_size);
    pq->size++;
    pq_sift_up(pq, pq->size - 1u);
    return 0;
}

/*
 * pop：移除堆顶。O(log n)
 * 对应 C++ std::priority_queue::pop。
 *
 * 若 out != NULL，将堆顶拷贝至 out（调用方管理 out 所指内容的生命周期）；
 * 若 out == NULL 且设置了 free_element，先析构堆顶元素再丢弃。
 *
 * 实现：将堆最后一个元素移到堆顶，size--，sift_down 恢复堆序。
 */
int pq_pop(pq_t *pq, void *out)
{
    char *top;
    char *last;

    if (!pq || pq->size == 0u) {
        return -1;
    }

    top  = pq_elem(pq, 0u);
    last = pq_elem(pq, pq->size - 1u);

    if (out) {
        memcpy(out, top, pq->element_size);
    } else if (pq->free_element) {
        pq->free_element(top);
    }

    pq->size -= 1u;
    if (pq->size > 0u) {
        memcpy(top, last, pq->element_size);
        pq_sift_down(pq, 0u);
    }

    return 0;
}

/*
 * top：查看堆顶，不修改队列。O(1)
 * 对应 C++ std::priority_queue::top（返回 const 引用）。
 */
int pq_top(const pq_t *pq, void *out)
{
    if (!pq || pq->size == 0u || !out) {
        return -1;
    }
    memcpy(out, pq->data, pq->element_size);
    return 0;
}

/*
 * top_ptr：零拷贝返回堆顶指针。O(1)
 * 注意：任何修改队列的操作（push/pop）可能导致内存重分配，指针随之失效。
 */
const void *pq_top_ptr(const pq_t *pq)
{
    if (!pq || pq->size == 0u) {
        return NULL;
    }
    return pq->data;
}

/* ──────────────────────────────────────────────────────────────────────────
 * 工具接口
 * ────────────────────────────────────────────────────────────────────────── */

size_t pq_size(const pq_t *pq)
{
    return pq ? pq->size : 0u;
}

bool pq_empty(const pq_t *pq)
{
    return !pq || pq->size == 0u;
}

void pq_clear(pq_t *pq)
{
    size_t i;

    if (!pq) {
        return;
    }
    if (pq->free_element) {
        for (i = 0u; i < pq->size; i++) {
            pq->free_element(pq_elem(pq, i));
        }
    }
    pq->size = 0u;
}
