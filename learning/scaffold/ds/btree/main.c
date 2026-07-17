/**
 * @file main.c
 * @brief m 叉 BTree 演示：插入、查找、分裂
 *
 * 编译: gcc -std=c11 -Wall -o btree main.c
 * 运行: ./btree
 */

#include <stdio.h>
#include <stdlib.h>

#define MAX_KEYS 3    /* 每个节点最多 3 个键 */
#define MAX_CHILDREN 4

/* BTree 节点 */
typedef struct Node {
    int keys[MAX_KEYS];
    int nkeys;
    struct Node *children[MAX_CHILDREN];
    int is_leaf;
} Node;

/* 创建节点 */
static Node *create_node(int is_leaf) {
    Node *n = calloc(1, sizeof(Node));
    n->nkeys = 0;
    n->is_leaf = is_leaf;
    return n;
}

/* 分裂满节点 */
static Node *split_child(Node *parent, int idx) {
    Node *child = parent->children[idx];
    Node *new_node = create_node(child->is_leaf);

    new_node->nkeys = MAX_KEYS / 2;
    for (int i = 0; i < MAX_KEYS / 2; i++)
        new_node->keys[i] = child->keys[i + MAX_KEYS / 2 + 1];

    if (!child->is_leaf)
        for (int i = 0; i <= MAX_KEYS / 2; i++)
            new_node->children[i] = child->children[i + MAX_KEYS / 2 + 1];

    child->nkeys = MAX_KEYS / 2;

    for (int i = parent->nkeys; i > idx; i--)
        parent->children[i + 1] = parent->children[i];
    parent->children[idx + 1] = new_node;

    for (int i = parent->nkeys; i > idx; i--)
        parent->keys[i] = parent->keys[i - 1];
    parent->keys[idx] = child->keys[MAX_KEYS / 2];
    parent->nkeys++;

    return new_node;
}

/* 插入非满节点 */
static void insert_nonfull(Node *node, int key) {
    int i = node->nkeys - 1;

    if (node->is_leaf) {
        while (i >= 0 && node->keys[i] > key) {
            node->keys[i + 1] = node->keys[i];
            i--;
        }
        node->keys[i + 1] = key;
        node->nkeys++;
    } else {
        while (i >= 0 && node->keys[i] > key) i--;
        i++;
        if (node->children[i]->nkeys == MAX_KEYS) {
            split_child(node, i);
            if (node->keys[i] < key) i++;
        }
        insert_nonfull(node->children[i], key);
    }
}

/* 插入键 */
static Node *insert(Node *root, int key) {
    if (!root) return create_node(1);

    if (root->nkeys == MAX_KEYS) {
        Node *new_root = create_node(0);
        new_root->children[0] = root;
        split_child(new_root, 0);
        int i = new_root->keys[0] < key ? 1 : 0;
        insert_nonfull(new_root->children[i], key);
        return new_root;
    }
    insert_nonfull(root, key);
    return root;
}

/* 查找键 */
static Node *search(Node *root, int key) {
    if (!root) return NULL;
    int i = 0;
    while (i < root->nkeys && key > root->keys[i]) i++;
    if (i < root->nkeys && key == root->keys[i]) return root;
    if (root->is_leaf) return NULL;
    return search(root->children[i], key);
}

/* 中序遍历 */
static void inorder(Node *root) {
    if (!root) return;
    for (int i = 0; i < root->nkeys; i++) {
        if (!root->is_leaf) inorder(root->children[i]);
        printf("%d ", root->keys[i]);
    }
    if (!root->is_leaf) inorder(root->children[root->nkeys]);
}

/* 释放树 */
static void free_tree(Node *root) {
    if (!root) return;
    if (!root->is_leaf)
        for (int i = 0; i <= root->nkeys; i++) free_tree(root->children[i]);
    free(root);
}

int main(void) {
    printf("=== m 叉 BTree 演示 (m=4) ===\n\n");
    Node *root = NULL;

    int keys[] = {10, 20, 5, 6, 12, 30, 7, 17};
    int n = sizeof(keys) / sizeof(keys[0]);

    printf("插入: ");
    for (int i = 0; i < n; i++) {
        printf("%d ", keys[i]);
        root = insert(root, keys[i]);
    }
    printf("\n\n遍历: ");
    inorder(root);
    printf("\n\n查找:\n");
    int test_keys[] = {6, 15, 30, 5};
    for (int i = 0; i < 4; i++) {
        printf("  %d -> %s\n", test_keys[i], search(root, test_keys[i]) ? "找到" : "未找到");
    }

    free_tree(root);
    return 0;
}
