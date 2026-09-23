/**
 * @file exec_hashjoin_px.h
 * @brief Gap#4 并行 Hash Join——build 串行建表 + probe 只读无锁并行
 *
 * open：排干 build_child 灌入单个 vecx_hashjoin_t（建表串行，扫描可经
 *       build_child 内部的 Exchange 并行）；随后向 px_scheduler 提交 dop
 *       个 worker，各自用 make_probe 独立实例化 probe 子树。
 * probe：build 完成后哈希表只读，worker 共享句柄并发 probe（无锁）。
 *       前置依据：vecx_hashjoin_probe 每调用使用局部状态，hj_find_slot 只读。
 * close：取消标志 + abort 队列 + 按节点完成计数回收 worker
 *       （每节点只等自己的 worker，不等全局调度器）。
 */
#ifndef DB_EXECUTOR_EXEC_HASHJOIN_PX_H
#define DB_EXECUTOR_EXEC_HASHJOIN_PX_H

#include "exec_node.h"
#include "exec_exchange.h"

#ifdef __cplusplus
extern "C" {
#endif

ExecNode *exec_create_hashjoin_px(ExecNode *build_child,
                                  px_subtree_fn make_probe, void *probe_ctx,
                                  int dop, int build_key_col, int probe_key_col);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_HASHJOIN_PX_H */
