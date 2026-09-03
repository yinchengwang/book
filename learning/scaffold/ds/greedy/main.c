/**
 * @file main.c
 * @brief 贪心算法演示
 *
 * 演示三种经典贪心问题：
 * 1. 活动选择 - 区间调度
 * 2. Huffman 编码 - 前缀编码与压缩
 * 3. 分数背包 - 可以部分选取
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ══════════════════════════════════════════════════════════════════════
 * 活动选择问题（区间调度）
 * ══════════════════════════════════════════════════════════════════════ */

/** 活动结构 */
typedef struct {
    int id;         /* 活动编号 */
    int start;      /* 开始时间 */
    int end;        /* 结束时间 */
} Activity;

/** 按结束时间排序的比较函数 */
static int cmp_activity(const void *a, const void *b)
{
    const Activity *aa = (const Activity *)a;
    const Activity *bb = (const Activity *)b;
    return aa->end - bb->end;
}

/**
 * 贪心选择：每次选结束最早且与已选活动兼容的活动
 * @param activities 活动数组
 * @param n 活动数量
 * @return 最大兼容活动数
 */
static int activity_selection(Activity *activities, int n)
{
    /* 按结束时间排序 */
    qsort(activities, (size_t)n, sizeof(Activity), cmp_activity);

    printf("\n  排序后活动:\n");
    for (int i = 0; i < n; i++) {
        printf("    [%d] 时间: %02d:00 - %02d:00\n",
               activities[i].id, activities[i].start, activities[i].end);
    }

    /* 贪心选择 */
    int count = 1;  /* 第一个活动必定选 */
    int last_end = activities[0].end;
    printf("\n  选择: [%d] (结束时间 %02d:00)\n", activities[0].id, last_end);

    for (int i = 1; i < n; i++) {
        if (activities[i].start >= last_end) {
            count++;
            last_end = activities[i].end;
            printf("  选择: [%d] (结束时间 %02d:00)\n", activities[i].id, last_end);
        }
    }

    return count;
}

/** 演示活动选择 */
static void demo_activity_selection(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 1: 活动选择问题（区间调度）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    Activity activities[] = {
        {1,  1,  4},
        {2,  3,  5},
        {3,  0,  6},
        {4,  5,  7},
        {5,  3,  9},
        {6,  5,  9},
        {7,  6, 10},
        {8,  8, 11},
        {9,  8, 12},
        {10, 2, 14},
        {11,12, 16}
    };
    int n = (int)(sizeof(activities) / sizeof(activities[0]));

    printf("\n  原始活动:\n");
    for (int i = 0; i < n; i++) {
        printf("    [%d] 时间: %02d:00 - %02d:00\n",
               activities[i].id, activities[i].start, activities[i].end);
    }

    int max_count = activity_selection(activities, n);
    printf("\n  最大兼容活动数: %d\n", max_count);
}

/* ══════════════════════════════════════════════════════════════════════
 * Huffman 编码
 * ══════════════════════════════════════════════════════════════════════ */

/** Huffman 树节点 */
typedef struct HuffNode {
    char ch;                 /* 字符（叶子节点） */
    int freq;                /* 频率 */
    struct HuffNode *left;
    struct HuffNode *right;
} HuffNode;

/** 优先队列节点 */
typedef struct PQNode {
    HuffNode *huff;
    struct PQNode *next;
} PQNode;

/** 优先队列（频率小的先出） */
typedef struct {
    PQNode *head;
} PriorityQueue;

static void pq_push(PriorityQueue *pq, HuffNode *huff)
{
    PQNode *node = (PQNode *)malloc(sizeof(PQNode));
    node->huff = huff;
    node->next = NULL;

    if (!pq->head || huff->freq < pq->head->huff->freq) {
        node->next = pq->head;
        pq->head = node;
    } else {
        PQNode *curr = pq->head;
        while (curr->next && curr->next->huff->freq <= huff->freq) {
            curr = curr->next;
        }
        node->next = curr->next;
        curr->next = node;
    }
}

static HuffNode *pq_pop(PriorityQueue *pq)
{
    if (!pq->head) return NULL;
    PQNode *node = pq->head;
    HuffNode *huff = node->huff;
    pq->head = node->next;
    free(node);
    return huff;
}

static int pq_empty(const PriorityQueue *pq)
{
    return pq->head == NULL;
}

/** 释放 Huffman 树 */
static void huffman_free(HuffNode *root)
{
    if (!root) return;
    huffman_free(root->left);
    huffman_free(root->right);
    free(root);
}

/** 生成编码表 */
static void huffman_gen_codes(HuffNode *root, char *prefix, int depth, int codes[256])
{
    if (!root) return;

    if (!root->left && !root->right) {
        prefix[depth] = '\0';
        codes[(unsigned char)root->ch] = depth;
        printf("  '%c' (freq=%d): %s (len=%d)\n", root->ch, root->freq,
               depth > 0 ? prefix : "(空)", depth);
        return;
    }

    prefix[depth] = '0';
    huffman_gen_codes(root->left, prefix, depth + 1, codes);
    prefix[depth] = '1';
    huffman_gen_codes(root->right, prefix, depth + 1, codes);
}

/** 构建 Huffman 树并生成编码 */
static void demo_huffman(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 2: Huffman 编码\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    /* 测试字符串: "beep boop beer!" */
    const char *text = "beep boop beer!";
    int freq[256] = {0};
    for (size_t i = 0; i < strlen(text); i++) {
        freq[(unsigned char)text[i]]++;
    }

    printf("\n  原文: \"%s\"\n", text);
    printf("\n  字符频率:\n");

    PriorityQueue pq = {NULL};
    for (int i = 0; i < 256; i++) {
        if (freq[i] > 0) {
            HuffNode *node = (HuffNode *)malloc(sizeof(HuffNode));
            node->ch = (char)i;
            node->freq = freq[i];
            node->left = node->right = NULL;
            pq_push(&pq, node);
            printf("  '%c': %d\n", i, freq[i]);
        }
    }

    /* 构建 Huffman 树 */
    while (!pq_empty(&pq) && pq.head->next) {
        HuffNode *left = pq_pop(&pq);
        HuffNode *right = pq_pop(&pq);
        HuffNode *parent = (HuffNode *)malloc(sizeof(HuffNode));
        parent->ch = '\0';
        parent->freq = left->freq + right->freq;
        parent->left = left;
        parent->right = right;
        pq_push(&pq, parent);
    }
    HuffNode *root = pq_pop(&pq);

    /* 生成编码 */
    int codes[256] = {0};
    char prefix[64];
    printf("\n  Huffman 编码:\n");
    huffman_gen_codes(root, prefix, 0, codes);

    /* 计算压缩率 */
    int original_bits = (int)strlen(text) * 8;
    int encoded_bits = 0;
    for (size_t i = 0; i < strlen(text); i++) {
        encoded_bits += codes[(unsigned char)text[i]];
    }
    printf("\n  原始大小: %d bits\n", original_bits);
    printf("  编码大小: %d bits\n", encoded_bits);
    printf("  压缩率:   %.1f%%\n", 100.0 * encoded_bits / original_bits);

    huffman_free(root);
}

/* ══════════════════════════════════════════════════════════════════════
 * 分数背包问题
 * ══════════════════════════════════════════════════════════════════════ */

/** 物品结构（分数背包） */
typedef struct {
    int id;
    int weight;
    int value;
    double ratio;  /* 单位重量价值 */
} Item;

/** 按单位价值排序 */
static int cmp_item_ratio(const void *a, const void *b)
{
    const Item *aa = (const Item *)a;
    const Item *bb = (const Item *)b;
    if (bb->ratio > aa->ratio) return 1;
    if (bb->ratio < aa->ratio) return -1;
    return 0;
}

/** 分数背包贪心算法 */
static double fractional_knapsack(Item *items, int n, int capacity)
{
    /* 按单位价值排序 */
    qsort(items, (size_t)n, sizeof(Item), cmp_item_ratio);

    double total_value = 0.0;
    int remain = capacity;

    printf("\n  按单位价值排序:\n");
    for (int i = 0; i < n; i++) {
        printf("    [%d] 重量=%d, 价值=%d, 单位价值=%.2f\n",
               items[i].id, items[i].weight, items[i].value, items[i].ratio);
    }

    printf("\n  贪心选择:\n");
    for (int i = 0; i < n && remain > 0; i++) {
        if (items[i].weight <= remain) {
            total_value += (double)items[i].value;
            remain -= items[i].weight;
            printf("    取全部 [%d]: +%.0f (剩余容量: %d)\n",
                   items[i].id, (double)items[i].value, remain);
        } else {
            double fraction = (double)remain / items[i].weight;
            total_value += items[i].value * fraction;
            printf("    取部分 [%d]: +%.2f (容量已满)\n",
                   items[i].id, items[i].value * fraction);
            remain = 0;
        }
    }

    return total_value;
}

/** 演示分数背包 */
static void demo_fractional_knapsack(void)
{
    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  示例 3: 分数背包问题\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    Item items[] = {
        {1, 10, 60, 0.0},
        {2, 20, 100, 0.0},
        {3, 30, 120, 0.0}
    };
    int n = (int)(sizeof(items) / sizeof(items[0]));
    int capacity = 50;

    for (int i = 0; i < n; i++) {
        items[i].ratio = (double)items[i].value / items[i].weight;
    }

    printf("\n  物品:\n");
    for (int i = 0; i < n; i++) {
        printf("    [%d] 重量=%d, 价值=%d\n", items[i].id, items[i].weight, items[i].value);
    }
    printf("  背包容量: %d\n", capacity);

    double max_value = fractional_knapsack(items, n, capacity);
    printf("\n  最大价值: %.2f\n", max_value);
}

/* ══════════════════════════════════════════════════════════════════════
 * 主函数
 * ══════════════════════════════════════════════════════════════════════ */

int main(void)
{
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║                  贪心算法 (Greedy Algorithm)                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");

    demo_activity_selection();
    demo_huffman();
    demo_fractional_knapsack();

    printf("\n" "═══════════════════════════════════════════════════════════════\n");
    printf("  贪心算法特点:\n");
    printf("    • 每步选择局部最优，期望得到全局最优\n");
    printf("    • 需证明贪心选择性质和最优子结构\n");
    printf("    • 适合：区间调度、Huffman、分数背包\n");
    printf("    • 不适合：0-1 背包（需 DP）\n");
    printf("═══════════════════════════════════════════════════════════════\n");

    return 0;
}
