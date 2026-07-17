/*
 * learning/scaffold/ds/heap/main.c
 * 二叉堆演示：基本堆操作 + 堆排序 + 优先级队列
 *
 * 编译: gcc -std=c11 -Wall -O2 main.c -o heap_demo
 * 运行: ./heap_demo
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ──────────────────────────────────────────────────────────────────────────
 * 宏定义
 * ────────────────────────────────────────────────────────────────────────── */
#define HEAP_SWAP_BUF_SIZE 256

/* ──────────────────────────────────────────────────────────────────────────
 * 堆结构定义（支持任意类型）
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    void   *data;           /* 数据指针 */
    size_t  size;           /* 当前元素数 */
    size_t  capacity;       /* 容量 */
    size_t  element_size;   /* 单元素大小 */
    int   (*compare)(const void *, const void *);  /* 比较函数 */
    void  (*free_elem)(void *);  /* 元素析构函数（可选） */
} heap_t;

/* ──────────────────────────────────────────────────────────────────────────
 * 辅助函数
 * ────────────────────────────────────────────────────────────────────────── */
static size_t parent(size_t i) { return (i - 1) / 2; }
static size_t left(size_t i)   { return 2 * i + 1; }
static size_t right(size_t i)  { return 2 * i + 2; }

static void *elem(const heap_t *h, size_t i) {
    return (char *)h->data + i * h->element_size;
}

static void heap_swap(heap_t *h, size_t i, size_t j) {
    char buf[HEAP_SWAP_BUF_SIZE];
    void *a = elem(h, i);
    void *b = elem(h, j);

    if (h->element_size <= HEAP_SWAP_BUF_SIZE) {
        memcpy(buf, a, h->element_size);
        memcpy(a, b, h->element_size);
        memcpy(b, buf, h->element_size);
    } else {
        char *tmp = malloc(h->element_size);
        if (!tmp) return;
        memcpy(tmp, a, h->element_size);
        memcpy(a, b, h->element_size);
        memcpy(b, tmp, h->element_size);
        free(tmp);
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * 堆操作：下沉
 * ────────────────────────────────────────────────────────────────────────── */
static void sift_down(heap_t *h, size_t i) {
    while (left(i) < h->size) {
        size_t best = i;
        size_t l = left(i), r = right(i);

        if (l < h->size && h->compare(elem(h, l), elem(h, best)) > 0)
            best = l;
        if (r < h->size && h->compare(elem(h, r), elem(h, best)) > 0)
            best = r;

        if (best == i) break;
        heap_swap(h, i, best);
        i = best;
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * 堆操作：上浮
 * ────────────────────────────────────────────────────────────────────────── */
static void sift_up(heap_t *h, size_t i) {
    while (i > 0) {
        size_t p = parent(i);
        if (h->compare(elem(h, i), elem(h, p)) > 0) {
            heap_swap(h, i, p);
            i = p;
        } else {
            break;
        }
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * Floyd 建堆算法：O(n)
 * ────────────────────────────────────────────────────────────────────────── */
static void heapify(heap_t *h) {
    if (h->size <= 1) return;
    for (size_t i = (h->size - 2) / 2; i > 0; i--)
        sift_down(h, i);
    sift_down(h, 0);
}

/* ──────────────────────────────────────────────────────────────────────────
 * 堆排序（原地）
 * ────────────────────────────────────────────────────────────────────────── */
void heap_sort(void *base, size_t n, size_t elem_size,
               int (*cmp)(const void *, const void *)) {
    if (n <= 1) return;

    /* 模拟 heap_t 结构进行就地操作 */
    heap_t h = { .data = base, .size = n, .capacity = n,
                 .element_size = elem_size, .compare = cmp, .free_elem = NULL };

    heapify(&h);

    while (h.size > 1) {
        heap_swap(&h, 0, h.size - 1);
        h.size--;
        sift_down(&h, 0);
    }
}

/* ──────────────────────────────────────────────────────────────────────────
 * 优先级队列（基于堆）
 * ────────────────────────────────────────────────────────────────────────── */
typedef struct {
    heap_t *heap;
} priority_queue_t;

/* ──────────────────────────────────────────────────────────────────────────
 * 演示程序
 * ────────────────────────────────────────────────────────────────────────── */
static int cmp_int(const void *a, const void *b) {
    int x = *(const int *)a, y = *(const int *)b;
    return (x > y) - (x < y);  /* 最大堆 */
}

int main(void) {
    printf("=== 二叉堆与优先级队列演示 ===\n\n");

    /* 1. 堆排序演示 */
    printf("【1】堆排序演示\n");
    int arr[] = {64, 34, 25, 12, 22, 11, 90, 5, 77, 30};
    size_t n = sizeof(arr) / sizeof(arr[0]);

    printf("排序前: ");
    for (size_t i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n");

    heap_sort(arr, n, sizeof(int), cmp_int);

    printf("排序后: ");
    for (size_t i = 0; i < n; i++) printf("%d ", arr[i]);
    printf("\n\n");

    /* 2. 优先级队列演示（模拟） */
    printf("【2】优先级队列演示（最大堆）\n");
    heap_t pq_heap = {0};
    pq_heap.data = malloc(16 * sizeof(int));
    pq_heap.capacity = 16;
    pq_heap.element_size = sizeof(int);
    pq_heap.compare = cmp_int;

    int vals[] = {15, 10, 20, 17, 8};
    printf("入队元素: ");
    for (size_t i = 0; i < 5; i++) {
        printf("%d ", vals[i]);
        memcpy(elem(&pq_heap, pq_heap.size), &vals[i], sizeof(int));
        pq_heap.size++;
        sift_up(&pq_heap, pq_heap.size - 1);
    }
    printf("\n");

    printf("出队顺序（从大到小）: ");
    while (pq_heap.size > 0) {
        int top;
        memcpy(&top, elem(&pq_heap, 0), sizeof(int));
        printf("%d ", top);
        heap_swap(&pq_heap, 0, pq_heap.size - 1);
        pq_heap.size--;
        if (pq_heap.size > 0) sift_down(&pq_heap, 0);
    }
    printf("\n");

    free(pq_heap.data);
    printf("\n=== 演示结束 ===\n");
    return 0;
}
