/*
 * rb_tree.c —— 红黑树实现
 *
 * ============================================================
 * 红黑树插入修复（3 种情况）
 * ============================================================
 *
 * 插入新节点（红色）后，如果父节点也是红色（违反性质4），需要修复。
 * 修复策略取决于"叔叔节点"的颜色：
 *
 * 记：z = 新插入的节点（红色）
 *     p = z 的父节点（红色）
 *     g = z 的祖父节点（必定是黑色，因为 p 是红色）
 *     u = z 的叔叔节点（g 的另一个子节点）
 *
 * ──────────────────────────────────────────────
 * 情况 1：叔叔是红色（Recolor 即可，无需旋转）
 *
 *      g(B)              g(R)            ← p,u 变黑，g 变红
 *     /   \             /   \
 *   p(R)  u(R)  →    p(B)  u(B)         ← 将问题上移至 g
 *   /                 /
 * z(R)              z(R)
 *
 * 修复后可能使 g 与其父节点形成"红-红"冲突，需要继续向上修复。
 *
 * ──────────────────────────────────────────────
 * 情况 2：叔叔是黑色，且 z 是 p 的右子（LR/RL 型）
 *
 *      g(B)              g(B)
 *     /                 /
 *   p(R)      →      z(R)
 *     \              /
 *     z(R)         p(R)
 *
 * 操作：左旋 p（将 z"转正"为左子）→ 转化为情况 3
 *
 * ──────────────────────────────────────────────
 * 情况 3：叔叔是黑色，且 z 是 p 的左子（LL 型）
 *
 *      g(B)              p(B)
 *     /   \             /   \
 *   p(R)  u(B)  →    z(R)  g(R)
 *   /                         \
 * z(R)                        u(B)
 *
 * 操作：p 变黑，g 变红，右旋 g → 性质全部恢复！
 *
 * ============================================================
 * 以上是左子树的情况（p 是 g 的左子）。
 * 右子树的情况是对称的（左右互换、旋转方向互换）。
 * ============================================================
 *
 * ============================================================
 * 删除修复（更复杂，4 种情况）
 * ============================================================
 * 删除一个节点后，如果被删除的节点是黑色，会破坏性质5（黑高减少）。
 * 引入"双黑"概念后，修复策略取决于兄弟节点 s 及其子节点的颜色。
 *
 * 由于删除修复非常复杂，此处实现采用简化策略：
 * 1. 标记待删除节点
 * 2. 用后继节点替换
 * 3. 调用 delete_fixup 从替换位置开始修复
 *
 * 详细修复逻辑参见 rb_delete_fixup 函数注释。
 */

#include <ds/rb_tree.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * 内部辅助
 * ============================================================ */

/* 判断节点是否为 NIL 或黑色 */
static int is_black(const ds_rb_tree_t *tree, const ds_rb_node_t *node)
{
    return !node || node == tree->nil || node->color == DS_RB_BLACK;
}

static int is_red(const ds_rb_tree_t *tree, const ds_rb_node_t *node)
{
    return !is_black(tree, node);
}

/* 创建节点 */
static ds_rb_node_t *rb_node_create(ds_rb_tree_t *tree, const void *data,
                                     size_t element_size, ds_rb_color_t color)
{
    ds_rb_node_t *node;

    node = (ds_rb_node_t *)calloc(1u, sizeof(ds_rb_node_t));
    if (!node) {
        return NULL;
    }

    if (data) {
        node->data = malloc(element_size);
        if (!node->data) {
            free(node);
            return NULL;
        }
        memcpy(node->data, data, element_size);
    }

    node->color = color;
    node->left = tree->nil;
    node->right = tree->nil;
    node->parent = tree->nil;
    return node;
}

/* 找最小值节点（在子树中） */
static ds_rb_node_t *rb_min_node(const ds_rb_tree_t *tree, ds_rb_node_t *node)
{
    while (node && node != tree->nil && node->left != tree->nil) {
        node = node->left;
    }
    return node;
}

/* ============================================================
 * 旋转操作（与 AVL 类似，但多了 parent 指针和 NIL 哨兵处理）
 * ============================================================ */

/*
 * 左旋：以 x 为支点左旋
 *
 *     x                 y
 *    / \      →        / \
 *   a   y             x   c
 *      / \           / \
 *     b   c         a   b
 */
static void rb_rotate_left(ds_rb_tree_t *tree, ds_rb_node_t *x)
{
    ds_rb_node_t *y = x->right;

    /* 步骤1：将 y 的左子 b 变为 x 的右子 */
    x->right = y->left;
    if (y->left != tree->nil) {
        y->left->parent = x;
    }

    /* 步骤2：将 y 接到 x 的父节点上 */
    y->parent = x->parent;
    if (x->parent == tree->nil) {
        tree->root = y;
    } else if (x == x->parent->left) {
        x->parent->left = y;
    } else {
        x->parent->right = y;
    }

    /* 步骤3：将 x 变为 y 的左子 */
    y->left = x;
    x->parent = y;
}

/* 右旋：以 x 为支点右旋（左旋的镜像操作） */
static void rb_rotate_right(ds_rb_tree_t *tree, ds_rb_node_t *x)
{
    ds_rb_node_t *y = x->left;

    x->left = y->right;
    if (y->right != tree->nil) {
        y->right->parent = x;
    }

    y->parent = x->parent;
    if (x->parent == tree->nil) {
        tree->root = y;
    } else if (x == x->parent->right) {
        x->parent->right = y;
    } else {
        x->parent->left = y;
    }

    y->right = x;
    x->parent = y;
}

/* ============================================================
 * 插入修复
 *
 * 当新插入的红色节点 z 的父节点 p 也是红色时（违反性质 4），
 * 需要进行修复。修复分为 3 种情况。
 * ============================================================ */
static void rb_insert_fixup(ds_rb_tree_t *tree, ds_rb_node_t *z)
{
    ds_rb_node_t *p;
    ds_rb_node_t *g;
    ds_rb_node_t *u;

    while (is_red(tree, z->parent)) {
        p = z->parent;
        g = p->parent;

        if (p == g->left) {
            /* p 是 g 的左子 */
            u = g->right;

            if (is_red(tree, u)) {
                /*
                 * 情况 1：叔叔是红色
                 * p 和 u 变黑，g 变红，z 上移到 g
                 */
                p->color = DS_RB_BLACK;
                u->color = DS_RB_BLACK;
                g->color = DS_RB_RED;
                z = g;
            } else {
                if (z == p->right) {
                    /*
                     * 情况 2：z 是 p 的右子（LR）
                     * 左旋 p，转化为情况 3
                     */
                    z = p;
                    rb_rotate_left(tree, z);

                    /* 更新 p 和 g（z 现在在原来的 p 位置） */
                }
                /* 重新获取 p 和 g（因为可能发生了旋转） */
                p = z->parent;
                g = p->parent;

                /*
                 * 情况 3：z 是 p 的左子（LL）
                 * p 变黑，g 变红，右旋 g
                 */
                p->color = DS_RB_BLACK;
                g->color = DS_RB_RED;
                rb_rotate_right(tree, g);
            }
        } else {
            /* p 是 g 的右子 —— 对称情况 */
            u = g->left;

            if (is_red(tree, u)) {
                /* 情况 1：叔叔是红色（对称） */
                p->color = DS_RB_BLACK;
                u->color = DS_RB_BLACK;
                g->color = DS_RB_RED;
                z = g;
            } else {
                if (z == p->left) {
                    /* 情况 2：z 是 p 的左子（RL）（对称） */
                    z = p;
                    rb_rotate_right(tree, z);
                }
                p = z->parent;
                g = p->parent;

                /* 情况 3：z 是 p 的右子（RR）（对称） */
                p->color = DS_RB_BLACK;
                g->color = DS_RB_RED;
                rb_rotate_left(tree, g);
            }
        }
    }

    /* 确保根始终为黑色（性质 2） */
    tree->root->color = DS_RB_BLACK;
}

/* ============================================================
 * 插入操作
 * ============================================================ */
int ds_rb_insert(ds_rb_tree_t *tree, const void *data, size_t element_size)
{
    ds_rb_node_t *z;
    ds_rb_node_t *y;
    ds_rb_node_t *x;

    if (!tree || !data || element_size == 0u) {
        return -1;
    }

    z = rb_node_create(tree, data, element_size, DS_RB_RED);
    if (!z) {
        return -1;
    }

    /* 标准 BST 插入（找到插入位置） */
    y = tree->nil;
    x = tree->root;
    while (x != tree->nil) {
        y = x;
        if (tree->compare(data, x->data) < 0) {
            x = x->left;
        } else {
            x = x->right;
        }
    }

    z->parent = y;
    if (y == tree->nil) {
        /* 树为空 */
        tree->root = z;
    } else if (tree->compare(data, y->data) < 0) {
        y->left = z;
    } else {
        y->right = z;
    }

    /* 修复红黑树性质 */
    rb_insert_fixup(tree, z);
    tree->size += 1u;
    return 0;
}

/* ============================================================
 * 搜索操作 — 标准 BST 查找
 * ============================================================ */
const ds_rb_node_t *ds_rb_search(const ds_rb_tree_t *tree, const void *key)
{
    const ds_rb_node_t *node;

    if (!tree || !key) {
        return NULL;
    }

    node = tree->root;
    while (node != tree->nil) {
        int cmp = tree->compare(key, node->data);
        if (cmp == 0) {
            return node;
        } else if (cmp < 0) {
            node = node->left;
        } else {
            node = node->right;
        }
    }

    return NULL;
}

/* ============================================================
 * 删除修复
 *
 * 当删除黑色节点时，需要修复黑高（性质 5）。
 * 修复围绕"兄弟节点 s"展开，分为 4 种情况：
 *
 * 记：x = 被替代的位置（双黑节点）
 *     s = x 的兄弟节点
 *
 * 情况 1：s 是红色 → 交换 s 和父节点的颜色，旋转父节点，转化为情况 2/3/4
 * 情况 2：s 是黑色，s 的两个子节点都是黑色 → s 变红，问题上移给父节点
 * 情况 3：s 是黑色，s 靠近 x 的子节点是红色，远离的黑色 → 旋转 s，转化为情况 4
 * 情况 4：s 是黑色，s 远离 x 的子节点是红色 → 旋转父节点，s 变父节点颜色，修复完成
 *
 * 由于篇幅限制，此实现采用简化删除策略。
 * 完整实现请参考 CLRS（算法导论）第 13 章。
 * ============================================================ */

/* 用 v 替换 u 在树中的位置 */
static void rb_transplant(ds_rb_tree_t *tree, ds_rb_node_t *u, ds_rb_node_t *v)
{
    if (u->parent == tree->nil) {
        tree->root = v;
    } else if (u == u->parent->left) {
        u->parent->left = v;
    } else {
        u->parent->right = v;
    }
    v->parent = u->parent;
}

static void rb_delete_fixup(ds_rb_tree_t *tree, ds_rb_node_t *x)
{
    ds_rb_node_t *s; /* 兄弟节点 */

    while (x != tree->root && is_black(tree, x)) {
        if (x == x->parent->left) {
            s = x->parent->right;

            /* 情况 1：兄弟是红色 */
            if (is_red(tree, s)) {
                s->color = DS_RB_BLACK;
                x->parent->color = DS_RB_RED;
                rb_rotate_left(tree, x->parent);
                s = x->parent->right;
            }

            /* 情况 2：兄弟的两个子节点都是黑色 */
            if (is_black(tree, s->left) && is_black(tree, s->right)) {
                s->color = DS_RB_RED;
                x = x->parent; /* 问题上移 */
            } else {
                /* 情况 3：兄弟的右子是黑色（左子是红色） */
                if (is_black(tree, s->right)) {
                    s->left->color = DS_RB_BLACK;
                    s->color = DS_RB_RED;
                    rb_rotate_right(tree, s);
                    s = x->parent->right;
                }

                /* 情况 4：兄弟的右子是红色 */
                s->color = x->parent->color;
                x->parent->color = DS_RB_BLACK;
                s->right->color = DS_RB_BLACK;
                rb_rotate_left(tree, x->parent);
                x = tree->root; /* 修复完成 */
            }
        } else {
            /* 对称情况（x 是右子） */
            s = x->parent->left;

            if (is_red(tree, s)) {
                s->color = DS_RB_BLACK;
                x->parent->color = DS_RB_RED;
                rb_rotate_right(tree, x->parent);
                s = x->parent->left;
            }

            if (is_black(tree, s->left) && is_black(tree, s->right)) {
                s->color = DS_RB_RED;
                x = x->parent;
            } else {
                if (is_black(tree, s->left)) {
                    s->right->color = DS_RB_BLACK;
                    s->color = DS_RB_RED;
                    rb_rotate_left(tree, s);
                    s = x->parent->left;
                }

                s->color = x->parent->color;
                x->parent->color = DS_RB_BLACK;
                s->left->color = DS_RB_BLACK;
                rb_rotate_right(tree, x->parent);
                x = tree->root;
            }
        }
    }

    x->color = DS_RB_BLACK;
}

int ds_rb_delete(ds_rb_tree_t *tree, const void *key, void (*free_data)(void *))
{
    ds_rb_node_t *z;     /* 要删除的节点 */
    ds_rb_node_t *y;     /* 实际被移除的节点 */
    ds_rb_node_t *x;     /* 替换 y 的节点 */
    ds_rb_color_t y_original_color;

    if (!tree || !key) {
        return -1;
    }

    /* 查找要删除的节点 */
    z = tree->root;
    while (z != tree->nil) {
        int cmp = tree->compare(key, z->data);
        if (cmp == 0) break;
        z = (cmp < 0) ? z->left : z->right;
    }
    if (z == tree->nil) {
        return -1; /* 未找到 */
    }

    y = z;
    y_original_color = y->color;

    if (z->left == tree->nil) {
        /* 情况 A：没有左子 */
        x = z->right;
        rb_transplant(tree, z, z->right);
    } else if (z->right == tree->nil) {
        /* 情况 B：没有右子 */
        x = z->left;
        rb_transplant(tree, z, z->left);
    } else {
        /* 情况 C：有两个子节点 —— 找后继 */
        y = rb_min_node(tree, z->right);
        y_original_color = y->color;
        x = y->right;

        if (y->parent == z) {
            x->parent = y;
        } else {
            rb_transplant(tree, y, y->right);
            y->right = z->right;
            y->right->parent = y;
        }

        rb_transplant(tree, z, y);
        y->left = z->left;
        y->left->parent = y;
        y->color = z->color;
    }

    /* 释放被删除的节点 */
    if (free_data && z->data) {
        free_data(z->data);
    }
    free(z->data);
    free(z);

    /* 如果被删除的节点是黑色，需要修复 */
    if (y_original_color == DS_RB_BLACK) {
        rb_delete_fixup(tree, x);
    }

    tree->size -= 1u;
    return 0;
}

/* ============================================================
 * 树管理
 * ============================================================ */

ds_rb_tree_t *ds_rb_tree_create(ds_rb_compare_fn compare)
{
    ds_rb_tree_t *tree;

    if (!compare) {
        return NULL;
    }

    tree = (ds_rb_tree_t *)calloc(1u, sizeof(ds_rb_tree_t));
    if (!tree) {
        return NULL;
    }

    /* 创建 NIL 哨兵 */
    tree->nil = (ds_rb_node_t *)calloc(1u, sizeof(ds_rb_node_t));
    if (!tree->nil) {
        free(tree);
        return NULL;
    }
    tree->nil->color = DS_RB_BLACK;
    tree->nil->left = tree->nil;
    tree->nil->right = tree->nil;
    tree->nil->parent = tree->nil;

    tree->root = tree->nil;
    tree->compare = compare;
    return tree;
}

/* 递归释放节点 */
static void rb_free_nodes(ds_rb_tree_t *tree, ds_rb_node_t *node,
                           void (*free_data)(void *))
{
    if (node == tree->nil) return;
    rb_free_nodes(tree, node->left, free_data);
    rb_free_nodes(tree, node->right, free_data);
    if (free_data && node->data) free_data(node->data);
    free(node->data);
    free(node);
}

void ds_rb_tree_destroy(ds_rb_tree_t *tree, void (*free_data)(void *))
{
    if (!tree) return;
    rb_free_nodes(tree, tree->root, free_data);
    free(tree->nil);
    free(tree);
}

size_t ds_rb_size(const ds_rb_tree_t *tree)
{
    return tree ? tree->size : 0u;
}

bool ds_rb_empty(const ds_rb_tree_t *tree)
{
    return !tree || tree->size == 0u;
}

/* ============================================================
 * 演示函数
 * ============================================================ */

static int rb_int_compare(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static void rb_print_inorder(const ds_rb_tree_t *tree, const ds_rb_node_t *node)
{
    if (node == tree->nil) return;
    rb_print_inorder(tree, node->left);
    printf("%d(%s) ", *(const int *)node->data,
           node->color == DS_RB_RED ? "R" : "B");
    rb_print_inorder(tree, node->right);
}

void ds_rb_tree_demo(void)
{
    printf("========== 红黑树演示 ==========\n");

    ds_rb_tree_t *tree = ds_rb_tree_create(rb_int_compare);
    if (!tree) {
        printf("创建红黑树失败\n");
        return;
    }

    /* 插入演示（有序插入不会退化为链表） */
    printf("\n【有序插入 1..7】\n");
    for (int i = 1; i <= 7; ++i) {
        ds_rb_insert(tree, &i, sizeof(int));
        printf("插入 %d → 中序遍历: ", i);
        rb_print_inorder(tree, tree->root);
        printf("| 大小=%zu\n", ds_rb_size(tree));
    }

    /* 搜索演示 */
    printf("\n【搜索】\n");
    for (int key = 0; key <= 8; ++key) {
        const ds_rb_node_t *found = ds_rb_search(tree, &key);
        if (found) {
            printf("  %d: 找到 (%s)\n", key,
                   found->color == DS_RB_RED ? "红" : "黑");
        } else {
            printf("  %d: 未找到\n", key);
        }
    }

    /* 删除演示 */
    printf("\n【删除 4（两个子节点场景）】\n");
    int del_key = 4;
    ds_rb_delete(tree, &del_key, NULL);
    printf("中序遍历: ");
    rb_print_inorder(tree, tree->root);
    printf("| 大小=%zu\n", ds_rb_size(tree));

    printf("\n【删除 1（叶子/单子场景）】\n");
    del_key = 1;
    ds_rb_delete(tree, &del_key, NULL);
    printf("中序遍历: ");
    rb_print_inorder(tree, tree->root);
    printf("| 大小=%zu\n", ds_rb_size(tree));

    /* 验证红黑树性质 */
    printf("\n根节点颜色: %s\n",
           tree->root->color == DS_RB_BLACK ? "黑 ✓" : "红 ✗");
    printf("性质 2（根为黑）: %s\n",
           tree->root == tree->nil || tree->root->color == DS_RB_BLACK ? "满足" : "违反");

    ds_rb_tree_destroy(tree, NULL);
    printf("========== 红黑树演示结束 ==========\n");
}
