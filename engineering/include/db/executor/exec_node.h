// engineering/include/db/executor/exec_node.h
#ifndef DB_EXECUTOR_EXEC_NODE_H
#define DB_EXECUTOR_EXEC_NODE_H

#include <stdint.h>
#include <stdbool.h>
#include "db/optimizer/optimizer.h"
#include "db/core/vector_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 执行节点基结构
 *
 * 每个算子实现需提供 open/next/close/reset 四个函数指针。
 * next() 返回 VectorBlock 指针，NULL 表示迭代结束。
 */
typedef struct ExecNode {
    plan_node_type_t node_type;     /**< 节点类型（PLAN_SCAN_SEQ 等） */
    struct ExecNode *left;          /**< 左子节点 */
    struct ExecNode *right;         /**< 右子节点（Join 用） */
    void *state;                    /**< 算子私有状态 */

    /**
     * @brief 初始化算子（相当于 PostgreSQL 的 ExecInit）
     * @return 0 成功，-1 失败
     */
    int (*open)(struct ExecNode *node);

    /**
     * @brief 获取下一批数据
     * @return VectorBlock 指针，NULL 表示迭代结束
     */
    struct VectorBlock *(*next)(struct ExecNode *node);

    /**
     * @brief 重置算子状态（用于迭代重启）
     */
    void (*reset)(struct ExecNode *node);

    /**
     * @brief 关闭算子，释放资源（相当于 PostgreSQL 的 ExecEnd）
     */
    void (*close)(struct ExecNode *node);
} ExecNode;

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_NODE_H */
