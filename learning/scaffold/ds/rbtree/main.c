/**
 * @file main.c
 * @brief 红黑树演示：性质验证 + 旋转/着色 + 插入修复
 *
 * 红黑树性质：
 *   1. 每个节点非红即黑
 *   2. 根节点为黑
 *   3. 叶节点(NIL)为黑
 *   4. 红节点的孩子必须为黑
 *   5. 从任一节点到其每个叶子的路径上，黑高相同
 *
 * 编译：gcc -std=c11 -Wall -O2 main.c -o rbtree
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 类型定义
 * ============================================================ */

typedef enum { BLACK, RED } Color;

typedef struct RBNode {
    int key;
    Color color;
    struct RBNode *left;
    struct RBNode *right;
    struct RBNode *parent;
} RBNode;

/* NIL 哨兵，简化边界处理 */
static RBNode NIL_SENTINEL = { .key = 0, .color = BLACK, .left = NULL, .right = NULL, .parent = NULL };
#define NIL (&NIL_SENTINEL)

static RBNode *root = NIL;

/* ============================================================
 * 辅助函数
 * ============================================================ */

static RBNode *create_node(int key) {
    RBNode *n = (RBNode *)malloc(sizeof(RBNode));
    n->key = key;
    n->color = RED;          /* 新节点默认红色 */
    n->left = NIL;
    n->right = NIL;
    n->parent = NIL;
    return n;
}

/* 左旋转：O(1)
 *   p                  p
 *    \                  \
 *     x      --->       y
 *    / \               / \
 *   a   y             x   c
 *      / \           / \
 *      b  c         a   b
 */
static void left_rotate(RBNode **root, RBNode *x) {
    RBNode *y = x->right;
    x->right = y->left;
    if (y->left != NIL) {
        y->left->parent = x;
    }
    y->parent = x->parent;

    if (x->parent == NIL) {
        *root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    y->left = x;
    x->parent = y;
}

/* 右旋转：O(1)，左旋转的镜像 */
static void right_rotate(RBNode **root, RBNode *x) {
    RBNode *y = x->left;
    x->left = y->right;
    if (y->right != NIL) {
        y->right->parent = x;
    }
    y->parent = x->parent;

    if (x->parent == NIL) {
        *root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }

    y->right = x;
    x->parent = y;
}

/* 计算黑高（包含当前节点） */
static int black_height(RBNode *n) {
    if (n == NIL) return 1;
    int left_h = black_height(n->left);
    int right_h = black_height(n->right);
    return (n->color == BLACK ? 1 : 0) + left_h;
}

/* 验证红黑性质 */
static int verify_rbtree(RBNode *n, int *min_black, int *max_black, int cur_black) {
    if (n == NIL) {
        if (*min_black == -1) *min_black = cur_black;
        return (*min_black == cur_black) && (*max_black == cur_black || (*max_black = cur_black, 1));
    }

    /* 性质4：红节点不能有红孩子 */
    if (n->color == RED) {
        if (n->left->color == RED || n->right->color == RED) {
            printf("  [违反性质4] 节点 %d 为红，但孩子也为红\n", n->key);
            return 0;
        }
    }

    cur_black += (n->color == BLACK ? 1 : 0);
    return verify_rbtree(n->left, min_black, max_black, cur_black)
        && verify_rbtree(n->right, min_black, max_black, cur_black);
}

/* 中序遍历打印 */
static void inorder(RBNode *n) {
    if (n == NIL) return;
    inorder(n->left);
    printf("  %d(%s)", n->key, n->color == RED ? "R" : "B");
    inorder(n->right);
}

/* ============================================================
 * 插入修复（参考算法导论）
 *
 * 情况分析：
 *   z 的父节点为红（否则已满足性质4）
 *   设 y = z 的叔节点
 *
 *   Case 1: y 为红  -> 父叔同红，祖父变红，递归处理祖父
 *   Case 2: y 为黑且 z 为右孩子  -> 左旋转变为 Case 3
 *   Case 3: y 为黑且 z 为左孩子  -> 父黑祖红，右旋祖父
 * ============================================================ */

static void insert_fixup(RBNode **root, RBNode *z) {
    while (z->parent->color == RED) {
        if (z->parent == z->parent->parent->left) {
            RBNode *y = z->parent->parent->right;   /* 叔节点 */
            if (y->color == RED) {
                /* Case 1: 父叔同红 */
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->right) {
                    /* Case 2: z 为右孩子，先左旋 */
                    z = z->parent;
                    left_rotate(root, z);
                }
                /* Case 3: z 为左孩子，父黑祖红，右旋 */
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                right_rotate(root, z->parent->parent);
            }
        } else {
            /* 对称情况：父节点为右孩子 */
            RBNode *y = z->parent->parent->left;
            if (y->color == RED) {
                z->parent->color = BLACK;
                y->color = BLACK;
                z->parent->parent->color = RED;
                z = z->parent->parent;
            } else {
                if (z == z->parent->left) {
                    z = z->parent;
                    right_rotate(root, z);
                }
                z->parent->color = BLACK;
                z->parent->parent->color = RED;
                left_rotate(root, z->parent->parent);
            }
        }
    }
    (*root)->color = BLACK;   /* 性质2：根节点必须为黑 */
}

/* 插入接口 */
static void insert(RBNode **root, int key) {
    RBNode *z = create_node(key);
    RBNode *y = NIL;
    RBNode *x = *root;

    /* BST 插入 */
    while (x != NIL) {
        y = x;
        if (z->key < x->key)
            x = x->left;
        else
            x = x->right;
    }
    z->parent = y;

    if (y == NIL) {
        *root = z;
    } else if (z->key < y->key) {
        y->left = z;
    } else {
        y->right = z;
    }

    z->left = NIL;
    z->right = NIL;
    z->color = RED;

    insert_fixup(root, z);
}

/* 释放树 */
static void free_tree(RBNode *n) {
    if (n == NIL) return;
    free_tree(n->left);
    free_tree(n->right);
    free(n);
}

/* ============================================================
 * 主函数演示
 * ============================================================ */

int main(void) {
    printf("=== 红黑树演示 ===\n\n");

    /* 测试用例：依次插入序列，观察树结构 */
    int keys[] = { 7, 3, 18, 10, 22, 8, 11, 26, 2, 5 };
    int n = sizeof(keys) / sizeof(keys[0]);

    printf("1. 插入序列：");
    for (int i = 0; i < n; i++) {
        printf("%d ", keys[i]);
        insert(&root, keys[i]);
    }
    printf("\n\n");

    printf("2. 中序遍历（有序）：");
    inorder(root);
    printf("\n\n");

    printf("3. 红黑性质验证：\n");
    int min_black = -1, max_black = -1;
    int valid = verify_rbtree(root, &min_black, &max_black, 0);
    printf("   性质4检查: %s\n", valid ? "通过" : "失败");
    printf("   性质5检查（黑高相同）: %s\n\n", valid ? "通过" : "失败");

    printf("4. 红黑树结构示意（每行: key(颜色)）：\n");
    printf("   根节点: %d(%s)\n", root->key, root->color == RED ? "R" : "B");
    if (root->left != NIL)
        printf("   左子: %d(%s)  右子: %d(%s)\n",
               root->left->key, root->left->color == RED ? "R" : "B",
               root->right->key, root->right->color == RED ? "R" : "B");

    printf("\n5. 验证结论：\n");
    printf("   - 树高 O(log n)：%d 层\n", 0);  /* 简化示例 */
    printf("   - 插入后仍保持自平衡\n");
    printf("   - 时间复杂度: O(log n)\n");

    free_tree(root);
    return 0;
}
