/**
 * @file exec_shard.h
 * @brief Gap#4 shard 读路径——谓词裁剪 + 每分片扇出
 *
 * px_shard_prune：按分片键谓词（int64 等值/范围）计算候选分片，
 *   谓词为空退化为全分片。内部接线 shard_route / shard_route_range（gap06）。
 * exec_create_shard_fanout：裁剪候选分片后，每分片经 open_shard 工厂建
 *   一个子扫描，全部喂给 Exchange（dop=候选数）——扇出与并行调度复用同一机制。
 *   返回节点底层是 Exchange（node_type = PLAN_EXCHANGE），但对上层是
 *   普通 ExecNode，pull 语义不变（行为透明）。
 * 写路由仍走 shard_coordinator_select_least_load（不在本头文件）。
 */
#ifndef DB_EXECUTOR_EXEC_SHARD_H
#define DB_EXECUTOR_EXEC_SHARD_H

#include "exec_node.h"
#include "db/sharding/sharding.h"
#include "db/vectorized/vectorized.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 纯函数裁剪：按分片键谓词计算候选分片。pred==NULL → 全分片。返回数量。
 * 返回 0 = 无候选分片（含 RANGE 路由下的域外等值键——shard_route 对域外键
 * 回退 shard 0，此处经 shard_route_range 复判后裁剪为空，调用方不得当成
 * "扫描 shard 0"）。 */
int px_shard_prune(const shard_router_t *router, const vecx_pred_t *pred,
                   int *out_ids, int max);

/* 每分片子扫描工厂 */
typedef ExecNode *(*px_shard_scan_fn)(int shard_id, void *ctx);

/* 扇出节点：裁剪候选分片，每分片一个 worker（Exchange LOCAL，dop=候选数） */
ExecNode *exec_create_shard_fanout(shard_router_t *router, const vecx_pred_t *pred,
                                   px_shard_scan_fn open_shard, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_SHARD_H */
