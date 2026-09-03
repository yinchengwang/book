/*
 * avl_tree.h —— AVL 树（自平衡二叉搜索树）
 *
 * ============================================================
 * 概述
 * ============================================================
 * AVL 树是第一种被发明的自平衡二叉搜索树（1962年，Adelson-Velsky & Landis）。
 * 它通过在每次插入/删除后检查平衡因子（balance factor）并执行旋转操作
 * 来保证树的高度始终为 O(log n)，从而确保所有操作都在 O(log n) 时间内完成。
 *
 * 平衡因子：BF(node) = height(left) - height(right)
 * AVL 性质：每个节点的 |BF| ≤ 1（即左右子树高度差不超过 1）
 *
 * ============================================================
 * 四种旋转场景
 * ============================================================
 *
 * 【LL（Left-Left）—— 右旋】
 *   插入发生在左子树的左子树中，导致 BF=+2
 *
 *        A (+2)            B ( 0)
 *       /                /   \
 *      B (+1)     →     C     A
 *     /
 *    C
 *
 *   操作：右旋 A（以 B 为新根）
 *
 * 【RR（Right-Right）—— 左旋】
 *   插入发生在右子树的右子树中，导致 BF=-2
 *
 *      A (-2)              B ( 0)
 *       \                /   \
 *        B (-1)   →     A     C
 *         \
 *          C
 *
 *   操作：左旋 A（以 B 为新根）
 *
 * 【LR（Left-Right）—— 左旋+右旋】
 *   插入发生在左子树的右子树中，导致 BF=+2
 *
 *        A (+2)           A (+2)           C ( 0)
 *       /                /               /   \
 *      B (-1)    →      C (+1)    →     B     A
 *       \              /
 *        C            B
 *
 *   操作：先左旋 B，再右旋 A
 *
 * 【RL（Right-Left）—— 右旋+左旋】
 *   插入发生在右子树的左子树中，导致 BF=-2
 *
 *      A (-2)           A (-2)           C ( 0)
 *       \                \             /   \
 *        B (+1)   →       C (-1)  →  A     B
 *       /                  \
 *      C                    B
 *
 *   操作：先右旋 B，再左旋 A
 *
 * ============================================================
 * 核心操作及时间复杂度
 * ============================================================
 * | 操作   | 复杂度  | 说明                                    |
 * |-------|--------|-----------------------------------------|
 * | insert| O(log n) | 最多 2 次旋转即可恢复平衡（LR/RL 需要 2 次） |
 * | delete| O(log n) | 可能需要 O(log n) 次旋转（沿路径向上传播）    |
 * | search| O(log n) | 与普通 BST 相同                           |
 *
 * ============================================================
 * 使用场景
 * ============================================================
 * - 需要频繁插入/删除且大量查找的场景（AVL 查找比红黑树略快）
 * - 数据库索引（需要严格平衡保证可预测的查找性能）
 * - 写入远少于读取的场景（AVL 旋转开销比红黑树大）
 *
 * ============================================================
 * 面试常见问题
 * ============================================================
 * 1. AVL 和红黑树的区别？→ AVL 更严格平衡（|BF|≤1 vs RB 宽松），查找更快但插入旋转更多。
 * 2. 为什么 AVL 树插入最多 2 次旋转？→ 一旦执行旋转，子树高度恢复，不需要向上传播。
 * 3. 为什么删除可能需要 O(log n) 次旋转？→ 删除后子树高度减少，可能逐级向上传播。
 */

#ifndef DS_AVL_TREE_H
#define DS_AVL_TREE_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_avl_node {
    void               *data;
    struct ds_avl_node *left;
    struct ds_avl_node *right;
    int                 height; /* 节点高度（叶子=1），用于快速计算平衡因子 */
} ds_avl_node_t;

/* 比较函数：标准三路比较，返回 <0, 0, >0 */
typedef int (*ds_avl_compare_fn)(const void *a, const void *b);

/* 插入：返回新的根节点 */
ds_avl_node_t *ds_avl_insert(ds_avl_node_t *root, const void *data,
                              size_t element_size, ds_avl_compare_fn compare);

/* 删除：返回新的根节点 */
ds_avl_node_t *ds_avl_delete(ds_avl_node_t *root, const void *key,
                              ds_avl_compare_fn compare, void (*free_data)(void *));

/* 搜索：返回找到的节点或 NULL */
const ds_avl_node_t *ds_avl_search(const ds_avl_node_t *root, const void *key,
                                    ds_avl_compare_fn compare);

/* 验证是否为合法 AVL 树 */
bool ds_avl_is_valid(const ds_avl_node_t *root, ds_avl_compare_fn compare);

/* 释放整棵树 */
void ds_avl_destroy(ds_avl_node_t *root, void (*free_data)(void *));

size_t ds_avl_height(const ds_avl_node_t *root);
size_t ds_avl_size(const ds_avl_node_t *root);

/* 演示函数 */
void ds_avl_tree_demo(void);

#ifdef __cplusplus
}
#endif

#endif /* DS_AVL_TREE_H */
