/**
 * greedy/main.c — 贪心算法典型应用（C11）
 *
 * 1. 活动选择问题（区间调度）
 * 2. 哈夫曼编码（贪心构造前缀码）
 * 3. 加油站问题（环形加油）
 *
 * 编译：make          # 生成 greedy
 * 运行：./greedy      # 运行所有示例
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ========== 1. 活动选择问题 ========== */
/* 每个活动有开始时间和结束时间，选择最多互不重叠的活动 */
typedef struct {
    int id;      /* 活动编号 */
    int start;   /* 开始时间 */
    int end;     /* 结束时间 */
} Activity;

/* 按结束时间排序（贪心选择依据） */
static int cmp_end(const void *a, const void *b)
{
    return ((const Activity *)a)->end - ((const Activity *)b)->end;
}

/* 贪心选择：每次选结束最早的兼容活动 */
static void activity_select(Activity acts[], int n)
{
    qsort(acts, (size_t)n, sizeof(Activity), cmp_end);
    printf("活动选择（最多活动）:\n");
    printf("  选中活动: %d (区间 [%d, %d])\n",
           acts[0].id, acts[0].start, acts[0].end);
    int last_end = acts[0].end;
    for (int i = 1; i < n; i++) {
        if (acts[i].start >= last_end) {
            printf("  选中活动: %d (区间 [%d, %d])\n",
                   acts[i].id, acts[i].start, acts[i].end);
            last_end = acts[i].end;
        }
    }
}

/* ========== 2. 哈夫曼编码（贪心构造最优前缀码）========== */
#define MAX_CHAR 128

typedef struct HNode {
    char ch;               /* 字符（仅叶子） */
    int freq;              /* 频次 */
    struct HNode *left;
    struct HNode *right;
} HNode;

/* 简单 Huffman 编码：输出码字表 */
/* 递归遍历并打印 */
static void print_codes(HNode *root, char buf[], int depth)
{
    if (!root) return;
    if (!root->left && !root->right) {
        buf[depth] = '\0';
        printf("  字符 '%c'(频次 %d): %s\n", root->ch, root->freq, buf);
        return;
    }
    if (root->left)  { buf[depth] = '0'; print_codes(root->left,  buf, depth + 1); }
    if (root->right) { buf[depth] = '1'; print_codes(root->right, buf, depth + 1); }
}

/* Min-Heap 辅助 */
typedef struct {
    HNode **data;
    int size;
    int cap;
} MinHeap;

static void heap_swap(HNode **a, HNode **b) { HNode *t = *a; *a = *b; *b = t; }

static void heap_push(MinHeap *h, HNode *node)
{
    if (h->size == h->cap) return;
    int i = h->size++;
    h->data[i] = node;
    while (i > 0 && h->data[i]->freq < h->data[(i - 1) / 2]->freq) {
        heap_swap(&h->data[i], &h->data[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}

static HNode *heap_pop(MinHeap *h)
{
    if (h->size == 0) return NULL;
    HNode *ret = h->data[0];
    h->data[0] = h->data[--h->size];
    int i = 0;
    while (1) {
        int smallest = i;
        int l = 2 * i + 1, r = 2 * i + 2;
        if (l < h->size && h->data[l]->freq < h->data[smallest]->freq) smallest = l;
        if (r < h->size && h->data[r]->freq < h->data[smallest]->freq) smallest = r;
        if (smallest == i) break;
        heap_swap(&h->data[i], &h->data[smallest]);
        i = smallest;
    }
    return ret;
}

static void build_huffman(const char *text)
{
    /* 统计频次 */
    int freq[MAX_CHAR] = {0};
    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++)
        freq[(unsigned char)text[i]]++;

    /* 初始化堆 */
    HNode *pool = calloc(MAX_CHAR * 2, sizeof(HNode));
    MinHeap heap = {
        .data = calloc((size_t)MAX_CHAR * 2, sizeof(HNode *)),
        .size = 0,
        .cap = MAX_CHAR * 2
    };
    int pool_idx = 0;
    for (int i = 0; i < MAX_CHAR; i++) {
        if (freq[i] > 0) {
            pool[pool_idx].ch = (char)i;
            pool[pool_idx].freq = freq[i];
            pool[pool_idx].left = pool[pool_idx].right = NULL;
            heap_push(&heap, &pool[pool_idx++]);
        }
    }

    /* 贪心合并：每次选频次最小的两个节点 */
    while (heap.size > 1) {
        HNode *a = heap_pop(&heap);
        HNode *b = heap_pop(&heap);
        pool[pool_idx].ch = 0;
        pool[pool_idx].freq = a->freq + b->freq;
        pool[pool_idx].left = a;
        pool[pool_idx].right = b;
        heap_push(&heap, &pool[pool_idx++]);
    }
    HNode *root = heap_pop(&heap);
    printf("\nHuffman编码（输入: \"%s\"）:\n", text);
    char buf[64];
    print_codes(root, buf, 0);

    free(heap.data);
    free(pool);
}

/* ========== 3. 加油站问题（环形加油）========== */
/* 环形路线有 n 个加油站，gas[i] 可加油量，cost[i] 到下一站耗油
 * 找能走完全程的起点，保证唯一解 */
static int can_complete_circuit(int gas[], int cost[], int n)
{
    int total = 0, curr = 0, start = 0;
    for (int i = 0; i < n; i++) {
        int diff = gas[i] - cost[i];
        total += diff;
        curr  += diff;
        if (curr < 0) {
            /* 当前起点不可行，尝试下一个 */
            start = i + 1;
            curr = 0;
        }
    }
    return total >= 0 ? start : -1;
}

/* ========== main: 运行所有示例 ========== */
int main(void)
{
    /* --- 活动选择 --- */
    Activity acts[] = {
        {1, 1, 4}, {2, 3, 5}, {3, 0, 6},
        {4, 5, 7}, {5, 3, 9}, {6, 5, 9},
        {7, 6, 10}, {8, 8, 11}, {9, 8, 12}, {10, 2, 14}
    };
    int n_acts = (int)(sizeof(acts) / sizeof(acts[0]));
    activity_select(acts, n_acts);

    /* --- 哈夫曼编码 --- */
    build_huffman("a fast runner need never be afraid of the dark");

    /* --- 加油站 --- */
    int gas[]  = {1, 2, 3, 4, 5};
    int cost[] = {3, 4, 5, 1, 2};
    int start = can_complete_circuit(gas, cost, 5);
    printf("\n加油站问题:\n");
    printf("  gas  = [1,2,3,4,5], cost = [3,4,5,1,2]\n");
    printf("  可行起点 = %d\n", start != -1 ? start : -1);

    return 0;
}
