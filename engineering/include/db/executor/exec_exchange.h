/**
 * @file exec_exchange.h
 * @brief Gap#4 Exchange 算子——并行"括号"（Volcano Exchange 模型）
 *
 * LOCAL 模式：open 时为每个 worker 同步调用一次 make_subtree(ctx)
 * 实例化独立子计划树（无共享状态），再经 px_scheduler 启动 dop 个
 * worker；worker 产出块推入 px_queue，next() 从队列拉块。
 *
 * 取消：close/destroy 时置 cancel_flag → px_queue_abort →
 * per-Exchange 完成计数（workers_outstanding + done_cond）等本 Exchange
 * 自己的 worker 退出，不等全局调度器——嵌套/共存 Exchange 不会自死锁；
 * 悬挂 worker 必被回收。worker 子树由 worker 负责 open/next/close/exec_destroy
 * （若拾取任务时已取消，子树从未 open，worker 只 exec_destroy 不 close）。
 */
#ifndef DB_EXECUTOR_EXEC_EXCHANGE_H
#define DB_EXECUTOR_EXEC_EXCHANGE_H

#include "exec_node.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef ExecNode *(*px_subtree_fn)(void *ctx);

ExecNode *exec_create_exchange(px_subtree_fn make_subtree, void *ctx, int dop);

/* 同 exec_create_exchange，附加 ctx 析构钩子：close 末尾对 ctx 调用一次
 * （工厂在 open 内同步调用，ctx 无需活到 worker 运行期） */
ExecNode *exec_create_exchange_ex(px_subtree_fn make_subtree, void *ctx, int dop,
                                  void (*ctx_destroy)(void *));

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_EXCHANGE_H */
