/**
 * @file main.c
 * @brief 跳表（Skip List）学习卡片 - 演示构建、随机层数、查找与插入
 *
 * 涵盖内容：
 * - 跳表节点结构与多层索引设计
 * - 随机层数生成（概率 p=0.5）
 * - 查找：从高层向低层逐层缩小范围
 * - 插入：先查找位置，再按随机层数插入各层
 * - 时间复杂度：O(log n) 平均情况
 *
 * 编译：gcc -std=c11 -Wall -o test main.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

/* 使用 srandom/random 替代 rand/random（更推荐） */
#define random() rand()

/* ==================== 常量定义 ==================== */

#define SKIPLIST_MAX_LEVEL 16  /* 最大层数 */
#define SKIPLIST_P 0.5         /* 每层晋升概率 */

/* ==================== 工具函数 ==================== */

/**
 * 打印分隔线
 */
static void print_separator(void)
{
    printf("  =========================================\n");
}

/**
 * 生成随机层数（几何分布）
 * 概率：层数为 k 的概率 = P^k * (1-P)
 * 即 level=1 概率 50%，level=2 概率 25%，level=3 概率 12.5%...
 *
 * @return 随机生成的层数 [1, SKIPLIST_MAX_LEVEL]
 */
static int random_level(void)
{
    int level = 1;
    while ((random() < (RAND_MAX * SKIPLIST_P)) && (level < SKIPLIST_MAX_LEVEL)) {
        level++;
    }
    return level;
}

/* ==================== 跳表节点定义 ==================== */

/**
 * 跳表节点
 * - 每个节点有多个前向指针（forward），形成多层索引
 * - forward[i] 指向第 i 层的下一个节点
 */
typedef struct SkipNode {
    int key;                    /* 键 */
    int value;                  /* 值 */
    int level;                  /* 该节点的层数（不含 level 0） */
    struct SkipNode **forward;  /* 前向指针数组，大小为 level */
} SkipNode;

/**
 * 跳表结构
 * - header: 头节点（哨兵），所有层的起点
 * - level: 当前最大层数
 * - size: 节点计数
 */
typedef struct {
    SkipNode *header;           /* 头节点（哨兵） */
    int level;                  /* 当前最大层数 */
    int size;                   /* 数据节点计数 */
} SkipList;

/* ==================== 跳表基本操作 ==================== */

/**
 * 创建跳表
 * 初始化头节点，所有层指向 NULL
 */
static SkipList *skiplist_create(void)
{
    SkipList *list = (SkipList *)malloc(sizeof(SkipList));
    if (!list) {
        return NULL;
    }

    /* 创建头节点，包含最大层数的前向指针 */
    list->header = (SkipNode *)malloc(sizeof(SkipNode) +
                                       SKIPLIST_MAX_LEVEL * sizeof(SkipNode *));
    if (!list->header) {
        free(list);
        return NULL;
    }

    list->header->key = 0;
    list->header->value = 0;
    list->header->level = SKIPLIST_MAX_LEVEL;
    list->header->forward = (SkipNode **)(list->header + 1);

    /* 初始化所有层的前向指针为 NULL */
    for (int i = 0; i < SKIPLIST_MAX_LEVEL; i++) {
        list->header->forward[i] = NULL;
    }

    list->level = 1;  /* 初始层数为 1 */
    list->size = 0;

    return list;
}

/**
 * 释放跳表
 */
static void skiplist_destroy(SkipList *list)
{
    if (!list) return;

    SkipNode *node = list->header->forward[0];
    while (node) {
        SkipNode *next = node->forward[0];
        free(node);
        node = next;
    }
    free(list->header);
    free(list);
}

/**
 * 打印跳表结构（可视化）
 */
static void skiplist_print(const char *label, const SkipList *list)
{
    printf("  %s (size=%d, level=%d):\n", label, list->size, list->level);

    /* 从高层向低层打印每一层 */
    for (int i = list->level - 1; i >= 0; i--) {
        printf("  L%d: ", i);
        SkipNode *node = list->header->forward[i];
        while (node) {
            printf("[%d:%d]", node->key, node->value);
            if (node->forward[i]) {
                printf(" -> ");
            }
            node = node->forward[i];
        }
        printf("\n");
    }
}

/* ==================== 跳表查找 ==================== */

/**
 * 跳表查找
 * 从高层向低层逐层搜索，每层最多遍历到目标位置
 *
 * @param list 跳表
 * @param key  要查找的键
 * @param prev 传出参数：保存每层查找路径上目标节点的前驱（可为 NULL）
 * @return 找到的节点，未找到返回 NULL
 */
static SkipNode *skiplist_search(SkipList *list, int key, SkipNode **prev[])
{
    SkipNode *current = list->header;

    /* 从最高层开始，逐层向下搜索 */
    for (int i = list->level - 1; i >= 0; i--) {
        /* 记录每层的前驱 */
        if (prev && *prev) {
            (*prev)[i] = current;
        }

        /* 在当前层向前搜索，直到遇到 >= key 的节点 */
        while (current->forward[i] && current->forward[i]->key < key) {
            current = current->forward[i];
            if (prev && *prev) {
                (*prev)[i] = current;
            }
        }
    }

    /* 最后一层（第 0 层），检查是否找到 */
    current = current->forward[0];
    if (current && current->key == key) {
        return current;
    }
    return NULL;
}

/**
 * 简化版查找（仅返回是否找到）
 */
static bool skiplist_contains(SkipList *list, int key)
{
    return skiplist_search(list, key, NULL) != NULL;
}

/* ==================== 跳表插入 ==================== */

/**
 * 跳表插入
 * 1. 生成随机层数
 * 2. 找到每一层需要插入的位置（前驱）
 * 3. 创建新节点，插入到各层
 *
 * @param list  跳表
 * @param key   键
 * @param value 值
 * @return 成功返回 0，失败返回 -1
 */
static int skiplist_insert(SkipList *list, int key, int value)
{
    SkipNode *update[SKIPLIST_MAX_LEVEL];  /* 每层的前驱节点 */
    SkipNode *current = list->header;

    /* 查找每一层的前驱节点 */
    for (int i = list->level - 1; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->key < key) {
            current = current->forward[i];
        }
        update[i] = current;
    }

    /* 检查是否已存在该键 */
    current = current->forward[0];
    if (current && current->key == key) {
        /* 键已存在，更新值 */
        current->value = value;
        return 0;
    }

    /* 生成随机层数 */
    int new_level = random_level();

    /* 如果新层数超过当前最大层数，需要更新头节点 */
    if (new_level > list->level) {
        for (int i = list->level; i < new_level; i++) {
            update[i] = list->header;
        }
        list->level = new_level;
    }

    /* 创建新节点 */
    SkipNode *node = (SkipNode *)malloc(sizeof(SkipNode) +
                                         new_level * sizeof(SkipNode *));
    if (!node) {
        return -1;
    }
    node->key = key;
    node->value = value;
    node->level = new_level;
    node->forward = (SkipNode **)(node + 1);

    /* 插入到每一层 */
    for (int i = 0; i < new_level; i++) {
        node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = node;
    }

    list->size++;
    return 0;
}

/* ==================== 演示函数 ==================== */

/**
 * 基本操作演示
 */
static void demo_basic_operations(void)
{
    printf("[skiplist] 跳表基本操作演示\n");
    print_separator();

    SkipList *list = skiplist_create();
    if (!list) {
        fprintf(stderr, "创建跳表失败\n");
        return;
    }

    /* 插入数据 */
    int keys[] = {3, 6, 7, 9, 12, 19, 25, 30};
    int n = sizeof(keys) / sizeof(keys[0]);

    printf("  插入数据: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", keys[i]);
        skiplist_insert(list, keys[i], keys[i] * 10);
    }
    printf("\n\n");

    skiplist_print("插入后", list);

    /* 查找操作 */
    printf("\n  查找操作:\n");
    int search_keys[] = {7, 15, 25};
    for (int i = 0; i < 3; i++) {
        int key = search_keys[i];
        bool found = skiplist_contains(list, key);
        printf("    查找 %d: %s\n", key, found ? "找到" : "未找到");
    }

    /* 插入更多数据 */
    printf("\n  插入更多数据: 5, 10, 20, 28\n");
    skiplist_insert(list, 5, 50);
    skiplist_insert(list, 10, 100);
    skiplist_insert(list, 20, 200);
    skiplist_insert(list, 28, 280);

    skiplist_print("插入更多数据后", list);

    /* 更新操作 */
    printf("\n  更新 key=25 的值为 250\n");
    skiplist_insert(list, 25, 250);
    printf("    更新后 key=25 的值: %d\n",
           skiplist_search(list, 25, NULL)->value);

    skiplist_destroy(list);
    printf("\n");
}

/**
 * 随机层数分布演示
 */
static void demo_random_level_distribution(void)
{
    printf("[skiplist] 随机层数分布演示\n");
    print_separator();

    srand(42);  /* 固定种子，保证可复现 */
    int counts[SKIPLIST_MAX_LEVEL + 1] = {0};
    int samples = 10000;

    printf("  生成 %d 个随机层数...\n", samples);

    for (int i = 0; i < samples; i++) {
        int level = random_level();
        counts[level]++;
    }

    printf("\n  层数分布统计（理论概率 P=0.5）:\n");
    printf("  ----------------------------------------\n");
    printf("  %-6s | %-8s | %-8s | %-8s\n", "层数", "实际次数", "实际比例", "理论比例");
    printf("  ----------------------------------------\n");

    for (int i = 1; i <= SKIPLIST_MAX_LEVEL; i++) {
        double ratio = (double)counts[i] / samples * 100;
        double theory = pow(SKIPLIST_P, i - 1) * (1 - SKIPLIST_P) * 100;
        if (counts[i] > 0) {  /* 只显示有数据的层 */
            printf("  %-6d | %-8d | %-6.2f%%   | %-6.2f%%\n",
                   i, counts[i], ratio, theory);
        }
    }
    printf("\n");

    /* 计算平均层数 */
    long total = 0;
    for (int i = 1; i <= SKIPLIST_MAX_LEVEL; i++) {
        total += (long)counts[i] * i;
    }
    printf("  平均层数: %.2f\n", (double)total / samples);
    printf("  理论平均层数: %.2f\n", 1.0 / (1 - SKIPLIST_P));
    printf("\n");
}

/**
 * 时间复杂度分析
 */
static void demo_complexity_analysis(void)
{
    printf("[skiplist] 时间复杂度分析\n");
    print_separator();

    printf("  | 操作   | 平均时间复杂度 | 最坏时间复杂度 |\n");
    printf("  |--------|----------------|----------------|\n");
    printf("  | 查找   | O(log n)       | O(n)           |\n");
    printf("  | 插入   | O(log n)       | O(n)           |\n");
    printf("  | 删除   | O(log n)       | O(n)           |\n");
    printf("\n");

    printf("  原理说明:\n");
    printf("  - 跳表通过多层索引实现类似二分查找的效果\n");
    printf("  - 每层节点数大约是下一层的 1/P（P=0.5 时约 1/2）\n");
    printf("  - 层数期望值为 O(log n)，因此操作复杂度为 O(log n)\n");
    printf("  - 最坏情况：所有节点都在同一层，退化为链表 O(n)\n");
    printf("\n");

    printf("  空间复杂度: O(n * 1/(1-P)) = O(2n) = O(n)\n");
    printf("  - 期望每节点有 1/(1-P) = 2 个前向指针\n");
    printf("  - 实际空间开销约为普通链表的 2 倍\n");
    printf("\n");
}

/* ==================== 主函数 ==================== */

int main(void)
{
    /* 初始化随机种子 */
    srand((unsigned int)time(NULL));

    printf("=== 数据结构: 跳表（Skip List） ===\n\n");

    /* 基本操作演示 */
    demo_basic_operations();

    /* 随机层数分布演示 */
    demo_random_level_distribution();

    /* 时间复杂度分析 */
    demo_complexity_analysis();

    printf("=== PASS ===\n");
    return 0;
}
