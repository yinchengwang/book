// engineering/include/db/executor/executor_framework.h
#ifndef DB_EXECUTOR_FRAMEWORK_H
#define DB_EXECUTOR_FRAMEWORK_H

#include "exec_node.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建执行树（Plan → ExecNode 转换）
 * @param plan 优化器输出的 plan_node_t 树
 * @return ExecNode 树，失败返回 NULL
 */
ExecNode *exec_create(const plan_node_t *plan);

/**
 * @brief 初始化执行树（自底向上调用 open）
 * @param root 执行树根节点
 * @return 0 成功，-1 失败
 */
int exec_open(ExecNode *root);

/**
 * @brief 获取下一批数据（从根节点驱动）
 * @param node 执行节点
 * @return VectorBlock 指针，NULL 表示迭代结束
 */
VectorBlock *exec_next(ExecNode *node);

/**
 * @brief 重置执行树（用于迭代重启）
 * @param root 执行树根节点
 */
void exec_reset(ExecNode *root);

/**
 * @brief 关闭执行树，释放资源（自顶向下调用 close）
 * @param root 执行树根节点
 */
void exec_close(ExecNode *root);

/**
 * @brief 销毁执行树，释放 ExecNode 树
 * @param root 执行树根节点
 */
void exec_destroy(ExecNode *root);

/**
 * @brief 一键执行接口
 * @param plan 优化器输出的 plan_node_t 树
 * @param has_result 输出：是否有结果
 * @return 结果 VectorBlock，has_result=0 时返回 NULL
 */
VectorBlock *exec_exec(const plan_node_t *plan, int *has_result);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_FRAMEWORK_H */
