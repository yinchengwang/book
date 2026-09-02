// engineering/src/db/executor/framework/executor_framework.c
#include "db/executor/executor_framework.h"
#include "db/executor/exec_node.h"
#include <stdlib.h>

/**
 * @brief 递归初始化（自底向上）
 */
static int exec_open_impl(ExecNode *node) {
    if (!node) return 0;

    // 先初始化子节点
    if (node->left && exec_open_impl(node->left) != 0) return -1;
    if (node->right && exec_open_impl(node->right) != 0) return -1;

    // 再初始化当前节点
    if (node->open && node->open(node) != 0) return -1;

    return 0;
}

int exec_open(ExecNode *root) {
    return exec_open_impl(root);
}

/**
 * @brief 获取下一批数据（委托给节点的 next 函数）
 */
VectorBlock *exec_next(ExecNode *node) {
    if (!node || !node->next) return NULL;
    return node->next(node);
}

/**
 * @brief 递归重置
 */
static void exec_reset_impl(ExecNode *node) {
    if (!node) return;
    if (node->reset) node->reset(node);
    exec_reset_impl(node->left);
    exec_reset_impl(node->right);
}

void exec_reset(ExecNode *root) {
    exec_reset_impl(root);
}

/**
 * @brief 递归关闭（自顶向下）
 */
static void exec_close_impl(ExecNode *node) {
    if (!node) return;

    // 先关闭当前节点
    if (node->close) node->close(node);

    // 再关闭子节点
    exec_close_impl(node->left);
    exec_close_impl(node->right);
}

void exec_close(ExecNode *root) {
    exec_close_impl(root);
}

/**
 * @brief 递归销毁
 */
static void exec_destroy_impl(ExecNode *node) {
    if (!node) return;

    exec_destroy_impl(node->left);
    exec_destroy_impl(node->right);

    if (node->state) free(node->state);
    free(node);
}

void exec_destroy(ExecNode *root) {
    exec_destroy_impl(root);
}

/**
 * @brief 一键执行
 */
VectorBlock *exec_exec(const plan_node_t *plan, int *has_result) {
    ExecNode *root = exec_create(plan);
    if (!root) {
        if (has_result) *has_result = 0;
        return NULL;
    }

    if (exec_open(root) != 0) {
        exec_destroy(root);
        if (has_result) *has_result = 0;
        return NULL;
    }

    VectorBlock *result = exec_next(root);
    if (has_result) *has_result = (result != NULL) ? 1 : 0;

    exec_close(root);
    exec_destroy(root);

    return result;
}
