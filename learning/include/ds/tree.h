/*
 * tree.h - 树便利函数封装
 *
 * 提供二叉树的高级操作，封装 binary_tree.h 的底层 API
 */
#ifndef ALGO_DS_TREE_H
#define ALGO_DS_TREE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// 树节点定义（兼容旧版 API）
typedef struct tree_node {
    int val;
    struct tree_node *left;
    struct tree_node *right;
} tree_node_t;

/* ========== 树构建 ========== */

/**
 * @brief 从数组构建二叉树（层序构建，-1 表示空节点）
 * @param arr 数组
 * @param size 数组大小
 * @return 树的根节点，失败返回 NULL
 */
tree_node_t *create_tree(int *arr, int size);

/* ========== BST 操作 ========== */

/**
 * @brief 向 BST 插入节点
 * @param root 树的根节点指针
 * @param val 要插入的值
 */
void bst_insert(tree_node_t **root, int val);

/* ========== 遍历（返回数组版本） ========== */

/**
 * @brief 前序遍历
 * @param root 树的根节点
 * @param return_size 输出参数，返回数组大小
 * @return 遍历结果数组（调用者需 free），失败返回 NULL
 */
int *pre_order(tree_node_t *root, uint32_t *return_size);

/**
 * @brief 中序遍历
 */
int *in_order(tree_node_t *root, uint32_t *return_size);

/**
 * @brief 后序遍历
 */
int *post_order(tree_node_t *root, uint32_t *return_size);

/**
 * @brief 层序遍历
 */
int *level_order(tree_node_t *root, uint32_t *return_size);

/* ========== 查询 ========== */

/**
 * @brief 获取树高度
 * @param root 树的根节点
 * @return 树的高度
 */
int get_tree_height(tree_node_t *root);

/* ========== 高级操作 ========== */

/**
 * @brief 判断是否存在从根到叶子的路径，和为指定值
 * @param root 树的根节点
 * @param sum 目标和
 * @return 是否存在这样的路径
 */
bool tree_has_sum_path(tree_node_t *root, int sum);

/**
 * @brief 判断二叉树是否镜像对称
 * @param root 树的根节点
 * @return 是否对称
 */
bool is_tree_symmetric(tree_node_t *root);

/* ========== 资源清理 ========== */

/**
 * @brief 销毁二叉树（递归释放所有节点）
 * @param root 树的根节点
 */
void tree_destroy(tree_node_t *root);

/* ========== 演示 ========== */

void ds_tree_demo(void);

#ifdef __cplusplus
}
#endif

#endif // ALGO_DS_TREE_H
