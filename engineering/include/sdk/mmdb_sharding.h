/**
 * @file mmdb_sharding.h
 * @brief 数据库分片 API（一致性哈希）
 *
 * 实现数据分片路由和跨分片查询。
 */
#ifndef MMDB_SHARDING_H
#define MMDB_SHARDING_H

#include "sdk/mmdb.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 分片句柄（前向声明） */
typedef struct mmdb_shard_s mmdb_shard_t;

/* 分片状态 */
typedef struct {
    uint32_t    shard_id;       /* 分片 ID */
    char        addr[256];      /* 分片地址 */
    uint64_t    key_count;      /* 键数量 */
    bool        alive;          /* 是否在线 */
} mmdb_shard_info_t;

/* 集群统计信息 */
typedef struct {
    uint32_t    shard_count;    /* 分片数量 */
    uint64_t    total_keys;     /* 总键数量 */
    uint64_t    avg_keys;       /* 平均键数量 */
    double      skew_factor;    /* 偏差因子（0-1，越小越均匀） */
} mmdb_sharding_stats_t;

/**
 * @brief 初始化分片集群
 * @param db     数据库句柄
 * @param shards JSON 格式的分片节点列表，格式：[{"id":1,"addr":"host:port"},...]
 * @return MMDB_OK 成功
 */
int mmdb_sharding_init(mmdb_t* db, const char* shards);

/**
 * @brief 获取键所属的分片
 * @param db       数据库句柄
 * @param key      键名
 * @param shard_id 输出的分片 ID
 * @return MMDB_OK 成功
 */
int mmdb_sharding_route(mmdb_t* db, const char* key, uint32_t* shard_id);

/**
 * @brief 获取分片信息
 * @param db     数据库句柄
 * @param shard_id 分片 ID
 * @param info   输出的分片信息
 * @return MMDB_OK 成功
 */
int mmdb_sharding_info(mmdb_t* db, uint32_t shard_id, mmdb_shard_info_t* info);

/**
 * @brief 手动迁移分片（负载均衡用）
 * @param db        数据库句柄
 * @param key       要迁移的键
 * @param from_shard 源分片 ID
 * @param to_shard   目标分片 ID
 * @return MMDB_OK 成功
 */
int mmdb_sharding_move(mmdb_t* db, const char* key, uint32_t from_shard, uint32_t to_shard);

/**
 * @brief 获取分片统计信息
 * @param db    数据库句柄
 * @param stats 输出的统计信息
 * @return MMDB_OK 成功
 */
int mmdb_sharding_stats(mmdb_t* db, mmdb_sharding_stats_t* stats);

/**
 * @brief 停止分片服务
 * @param db 数据库句柄
 * @return MMDB_OK 成功
 */
int mmdb_sharding_stop(mmdb_t* db);

/**
 * @brief 添加分片（在线扩容）
 * @param db     数据库句柄
 * @param shard  新分片信息 JSON：{"id":N,"addr":"host:port"}
 * @return MMDB_OK 成功
 */
int mmdb_sharding_add(mmdb_t* db, const char* shard);

/**
 * @brief 移除分片（在线缩容）
 * @param db       数据库句柄
 * @param shard_id 要移除的分片 ID
 * @return MMDB_OK 成功
 */
int mmdb_sharding_remove(mmdb_t* db, uint32_t shard_id);

#ifdef __cplusplus
}
#endif

#endif /* MMDB_SHARDING_H */
