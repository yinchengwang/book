// engineering/include/db/executor/exec_shard.h
#ifndef DB_EXECUTOR_EXEC_SHARD_H
#define DB_EXECUTOR_EXEC_SHARD_H

#include "db/executor/exec_node.h"
#include "db/sharding/shard_coordinator.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 创建分片扫描 ExecNode
 */
ExecNode *exec_create_shard_scan(shard_coordinator_t *coord,
                                  const void *key, size_t key_len);

#ifdef __cplusplus
}
#endif

#endif /* DB_EXECUTOR_EXEC_SHARD_H */