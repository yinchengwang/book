/*
 * skip_list.c —— 跳表实现
 *
 * ============================================================
 * 数据结构内部定义
 * ============================================================
 *
 *   struct ds_skip_node {
 *       void                *data;
 *       size_t               level;  // 此节点的层高
 *       struct ds_skip_node *forward[];  // 柔性数组：每层的下一个节点指针
 *   };
 *
 *   struct ds_skip_list {
 *       ds_skip_node_t       *header;  // 头哨兵（最高层）
 *       size_t                level;   // 当前最大层数
 *       size_t                size;    // 元素个数
 *       size_t                element_size;
 *       ds_skiplist_compare_fn compare;
 *   };
 *
 * ============================================================
 * 随机层高生成
 * ============================================================
 *
 * 抛硬币策略：不断"抛硬币"直到出现反面或达到最大层数。
 * 概率 p=0.5：
 *   level=1 概率 1/2
 *   level=2 概率 1/4
 *   level=3 概率 1/8
 *   ...
 *
 * 期望层高 = 1/(1-p) = 2（当 p=0.5 时）
 * 最大层高 = O(log n)，通常设置上限为 16 或 32。
 *
 * ============================================================
 * 查找操作
 * ============================================================
 *
 * 从最高层开始，每层向右尽可能地前进，遇到更大的值时下降到下一层。
 *
 *   search(key):
 *     curr = header
 *     for i = max_level-1 down to 0:
 *       while curr->forward[i] != NIL && curr->forward[i]->data < key:
 *         curr = curr->forward[i]
 *     curr = curr->forward[0]
 *     if curr != NIL && curr->data == key: return curr
 *     return NULL
 *
 * ============================================================
 * 插入操作
 * ============================================================
 *
 *   1. 随机生成新节点的层高 new_level
 *   2. 在每层找到插入位置（同查找过程），记录每层的"前驱节点"
 *   3. 在各层插入新节点（修改前驱的 forward 指针）
 *
 *   关键：使用 update[] 数组记录每层的前驱节点。
 *
 * ============================================================
 * 插入示例
 * ============================================================
 *
 *   插入 8（随机 level=2）到跳表：
 *
 *   原始: Level 1: HEAD → 2 → 5 → 12 → NIL
 *         Level 0: HEAD → 2 → 5 → 12 → NIL
 *
 *   查找过程记录 update[]：
 *     Level 1: update[1] = 节点5（5 < 8 < 12）
 *     Level 0: update[0] = 节点5
 *
 *   插入后:
 *     Level 1: HEAD → 2 → 5 → 8 → 12 → NIL
 *     Level 0: HEAD → 2 → 5 → 8 → 12 → NIL
 *
 *   新节点 forward：
 *     forward[1] = update[1]->forward[1] = 节点12
 *     forward[0] = update[0]->forward[0] = 节点12
 *   update[1]->forward[1] = 新节点
 *   update[0]->forward[0] = 新节点
 */

#include <ds/skip_list.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 跳表节点 —— forward 是柔性数组 */
typedef struct ds_skip_node {
    void                *data;
    struct ds_skip_node *forward[]; /* 每层一个指针 */
} ds_skip_node_t;

struct ds_skip_list {
    ds_skip_node_t       *header;
    size_t                max_level;    /* 当前最大层数 */
    size_t                size;
    size_t                element_size;
    ds_skiplist_compare_fn compare;
};

/* 随机层高（抛硬币策略） */
static size_t random_level(void)
{
    size_t level = 1u;
    while ((rand() & 1) && level < DS_SKIP_LIST_MAX_LEVEL) {
        ++level;
    }
    return level;
}

/* 创建节点 */
static ds_skip_node_t *skip_node_create(const void *data, size_t element_size, size_t level)
{
    ds_skip_node_t *node;

    node = (ds_skip_node_t *)calloc(1u, sizeof(ds_skip_node_t) + sizeof(ds_skip_node_t *) * level);
    if (!node) return NULL;

    node->data = malloc(element_size);
    if (!node->data) {
        free(node);
        return NULL;
    }
    if (data) {
        memcpy(node->data, data, element_size);
    }
    return node;
}

ds_skiplist_t *ds_skiplist_create(size_t element_size, ds_skiplist_compare_fn compare)
{
    ds_skiplist_t *list;

    if (element_size == 0u || !compare) return NULL;

    list = (ds_skiplist_t *)calloc(1u, sizeof(ds_skiplist_t));
    if (!list) return NULL;

    /* 头哨兵拥有最高层数 */
    list->header = skip_node_create(NULL, 0u, DS_SKIP_LIST_MAX_LEVEL);
    if (!list->header) {
        free(list);
        return NULL;
    }
    /* 哨兵的 data 为空 */

    list->max_level = 1u;
    list->element_size = element_size;
    list->compare = compare;
    return list;
}

void ds_skiplist_destroy(ds_skiplist_t *list)
{
    if (!list) return;

    ds_skip_node_t *curr = list->header->forward[0];
    while (curr) {
        ds_skip_node_t *next = curr->forward[0];
        free(curr->data);
        free(curr);
        curr = next;
    }
    free(list->header);
    free(list);
}

/* 查找：返回目标节点或 NULL */
const void *ds_skiplist_search(const ds_skiplist_t *list, const void *key)
{
    ds_skip_node_t *curr;
    int             cmp;

    if (!list || !key) return NULL;

    curr = list->header;
    /* 从最高层向下、向右搜索 */
    for (size_t i = list->max_level; i > 0u; --i) {
        size_t level = i - 1u;
        while (curr->forward[level]) {
            cmp = list->compare(key, curr->forward[level]->data);
            if (cmp > 0) {
                curr = curr->forward[level]; /* key 更大，继续向右 */
            } else if (cmp == 0) {
                return curr->forward[level]->data; /* 找到了 */
            } else {
                break; /* key 更小，下降到下一层 */
            }
        }
    }
    return NULL;
}

bool ds_skiplist_contains(const ds_skiplist_t *list, const void *key)
{
    return ds_skiplist_search(list, key) != NULL;
}

/*
 * 插入操作
 *
 * update[i] 记录在第 i 层，新节点应该插在哪个节点后面。
 */
int ds_skiplist_insert(ds_skiplist_t *list, const void *element)
{
    ds_skip_node_t *update[DS_SKIP_LIST_MAX_LEVEL];
    ds_skip_node_t *curr;
    size_t          new_level;
    int             cmp;

    if (!list || !element) return -1;

    /* 步骤1：在每层找到插入位置 */
    curr = list->header;
    for (size_t i = list->max_level; i > 0u; --i) {
        size_t level = i - 1u;
        while (curr->forward[level]) {
            cmp = list->compare(element, curr->forward[level]->data);
            if (cmp > 0) {
                curr = curr->forward[level];
            } else {
                break;
            }
        }
        update[level] = curr;
    }

    /* 步骤2：随机生成层高 */
    new_level = random_level();

    /* 如果新节点层高超过当前最大层，需要更新 header 的相关层 */
    if (new_level > list->max_level) {
        for (size_t i = list->max_level; i < new_level; ++i) {
            update[i] = list->header;
        }
        list->max_level = new_level;
    }

    /* 步骤3：创建新节点 */
    ds_skip_node_t *new_node = skip_node_create(element, list->element_size, new_level);
    if (!new_node) return -1;

    /* 步骤4：在各层插入新节点 */
    for (size_t i = 0u; i < new_level; ++i) {
        new_node->forward[i] = update[i]->forward[i];
        update[i]->forward[i] = new_node;
    }

    list->size += 1u;
    return 0;
}

/*
 * 删除操作
 *
 * 在各层找到目标节点的前驱，将前驱的 forward 指针绕过目标节点。
 */
int ds_skiplist_delete(ds_skiplist_t *list, const void *key)
{
    ds_skip_node_t *update[DS_SKIP_LIST_MAX_LEVEL];
    ds_skip_node_t *curr;
    ds_skip_node_t *target;

    if (!list || !key) return -1;

    curr = list->header;
    memset(update, 0, sizeof(update));
    target = NULL;

    /* 在各层查找要删除的节点 */
    for (size_t i = list->max_level; i > 0u; --i) {
        size_t level = i - 1u;
        while (curr->forward[level]) {
            int cmp = list->compare(key, curr->forward[level]->data);
            if (cmp > 0) {
                curr = curr->forward[level];
            } else if (cmp == 0) {
                target = curr->forward[level];
                update[level] = curr;
                break;
            } else {
                break;
            }
        }
        update[level] = curr;
    }

    if (!target) return -1; /* 未找到 */

    /* 从各层删除目标节点 */
    for (size_t i = 0u; i < list->max_level; ++i) {
        if (update[i] && update[i]->forward[i] == target) {
            update[i]->forward[i] = target->forward[i];
        }
    }

    /* 调整最大层数 */
    while (list->max_level > 1u && !list->header->forward[list->max_level - 1u]) {
        list->max_level -= 1u;
    }

    free(target->data);
    free(target);
    list->size -= 1u;
    return 0;
}

size_t ds_skiplist_size(const ds_skiplist_t *list)
{
    return list ? list->size : 0u;
}

bool ds_skiplist_empty(const ds_skiplist_t *list)
{
    return !list || list->size == 0u;
}

/* ============================================================
 * 演示函数
 * ============================================================ */

static int skiplist_int_compare(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

void ds_skip_list_demo(void)
{
    printf("========== 跳表演示 ==========\n");

    ds_skiplist_t *list = ds_skiplist_create(sizeof(int), skiplist_int_compare);
    if (!list) { printf("创建失败\n"); return; }

    printf("\n【插入 3, 6, 7, 9, 12, 19, 17】\n");
    int nums[] = {3, 6, 7, 9, 12, 19, 17};
    for (int i = 0; i < 7; ++i) {
        ds_skiplist_insert(list, &nums[i]);
    }
    printf("当前大小: %zu\n", ds_skiplist_size(list));

    printf("\n【查找】\n");
    for (int key = 3; key <= 20; key += 4) {
        const int *found = (const int *)ds_skiplist_search(list, &key);
        if (found) {
            printf("  %d: 找到 (值=%d)\n", key, *found);
        } else {
            printf("  %d: 未找到\n", key);
        }
    }

    printf("\n【删除 7 和 19】\n");
    int del1 = 7, del2 = 19;
    ds_skiplist_delete(list, &del1);
    ds_skiplist_delete(list, &del2);
    printf("删除后大小: %zu\n", ds_skiplist_size(list));
    printf("7 还在吗？%s\n", ds_skiplist_contains(list, &del1) ? "是" : "否");
    printf("12 还在吗？%s\n", ds_skiplist_contains(list, &nums[4]) ? "是" : "否");

    ds_skiplist_destroy(list);
    printf("========== 跳表演示结束 ==========\n");
}
