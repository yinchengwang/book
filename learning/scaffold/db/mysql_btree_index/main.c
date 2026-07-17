/*
 * B+树索引原理演示
 * 对比 B-Tree（内部节点存数据）与 B+Tree（数据全在叶子层）
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ORDER 5  /* B-Tree阶数，每节点最多 ORDER-1 个key */

/* B-Tree 节点结构（对应工程源码 btree_node_t） */
typedef struct btree_node {
    int keys[ORDER - 1];              /* 键数组 */
    int values[ORDER - 1];            /* 值数组 */
    struct btree_node *children[ORDER]; /* 子指针数组 */
    int n;                            /* 当前键数量 */
    bool is_leaf;                     /* 是否叶子 */
} btree_node_t;

/* 创建节点 */
btree_node_t *create_node(bool is_leaf) {
    btree_node_t *node = (btree_node_t *)calloc(1, sizeof(btree_node_t));
    node->is_leaf = is_leaf;
    return node;
}

/* 二分查找 key 位置 */
int find_key_pos(btree_node_t *node, int key) {
    int pos = 0;
    while (pos < node->n && key > node->keys[pos]) pos++;
    return pos;
}

/* 节点分裂：中间键上浮 */
void split_child(btree_node_t *parent, int pos) {
    btree_node_t *child = parent->children[pos];
    btree_node_t *new_node = create_node(child->is_leaf);
    int mid = (ORDER - 1) / 2;

    /* 新节点接收后半部分 */
    for (int i = mid + 1; i < ORDER - 1; i++) {
        new_node->keys[i - mid - 1] = child->keys[i];
        new_node->values[i - mid - 1] = child->values[i];
    }
    if (!child->is_leaf)
        for (int i = mid + 1; i < ORDER; i++)
            new_node->children[i - mid - 1] = child->children[i];
    new_node->n = ORDER - 1 - mid - 1;
    child->n = mid;

    /* 父节点插入中间键 */
    for (int i = parent->n; i > pos; i--) {
        parent->keys[i] = parent->keys[i - 1];
        parent->children[i + 1] = parent->children[i];
    }
    parent->keys[pos] = child->keys[mid];
    parent->values[pos] = child->values[mid];
    parent->children[pos + 1] = new_node;
    parent->n++;
}

/* 插入到未满节点 */
void insert_nonfull(btree_node_t *node, int key, int value) {
    int pos = find_key_pos(node, key);
    if (node->is_leaf) {
        for (int i = node->n; i > pos; i--) {
            node->keys[i] = node->keys[i - 1];
            node->values[i] = node->values[i - 1];
        }
        node->keys[pos] = key;
        node->values[pos] = value;
        node->n++;
    } else {
        if (node->children[pos]->n == ORDER - 1) {
            split_child(node, pos);
            if (key > node->keys[pos]) pos++;
        }
        insert_nonfull(node->children[pos], key, value);
    }
}

/* 插入入口 */
btree_node_t *insert(btree_node_t *root, int key, int value) {
    if (root->n == ORDER - 1) {
        btree_node_t *new_root = create_node(false);
        new_root->children[0] = root;
        split_child(new_root, 0);
        insert_nonfull(new_root, key, value);
        return new_root;
    }
    insert_nonfull(root, key, value);
    return root;
}

/* 查找 */
bool search(btree_node_t *node, int key, int *value_out) {
    if (!node) return false;
    int pos = find_key_pos(node, key);
    if (pos < node->n && node->keys[pos] == key) {
        *value_out = node->values[pos];
        return true;
    }
    return node->is_leaf ? false : search(node->children[pos], key, value_out);
}

/* 范围查询：中序遍历 */
void range_query(btree_node_t *node, int low, int high, int *results, int *count) {
    if (!node) return;
    int pos = 0;
    while (pos < node->n && node->keys[pos] < low) pos++;
    if (!node->is_leaf) range_query(node->children[pos], low, high, results, count);
    while (pos < node->n && node->keys[pos] <= high) {
        results[(*count)++] = node->keys[pos];
        if (!node->is_leaf) range_query(node->children[pos + 1], low, high, results, count);
        pos++;
    }
    if (!node->is_leaf && pos <= node->n) range_query(node->children[pos], low, high, results, count);
}

int main(void) {
    btree_node_t *root = create_node(true);
    int keys[] = {10, 20, 5, 6, 12, 30, 7, 17, 8, 22, 25};

    /* 插入演示 */
    printf("=== B-Tree 索引演示 ===\n");
    for (int i = 0; i < 11; i++) root = insert(root, keys[i], keys[i] * 100);

    /* 查找演示 */
    int value;
    printf("查找 12: %s\n", search(root, 12, &value) ? "找到" : "未找到");
    printf("查找 100: %s\n", search(root, 100, &value) ? "找到" : "未找到");

    /* 范围查询 */
    printf("\n=== 范围查询 [10, 20] ===\n");
    int results[20], count = 0;
    range_query(root, 10, 20, results, &count);
    printf("结果: ");
    for (int i = 0; i < count; i++) printf("%d ", results[i]);
    printf("\n共 %d 个键\n", count);

    printf("\n=== 索引概念 ===\n");
    printf("唯一索引: key 重复拒绝插入\n");
    printf("非唯一索引: (key, row_id) 复合键\n");
    printf("聚簇索引: 数据在叶子（InnoDB主键）\n");
    printf("覆盖索引: 辅助索引包含查询列，避免回表\n");

    return 0;
}