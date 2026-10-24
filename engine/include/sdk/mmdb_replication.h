/**
 * @file mmdb_replication.h
 * @brief 数据库复制 API（Raft 协议）
 *
 * 实现 Leader-Follower 主从复制，支持自动故障转移。
 */
#ifndef MMDB_REPLICATION_H
#define MMDB_REPLICATION_H

#include "sdk/mmdb.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 复制角色 */
typedef enum {
    MMDB_REPLICA_LEADER,    /* 主节点 */
    MMDB_REPLICA_FOLLOWER,  /* 从节点 */
    MMDB_REPLICA_CANDIDATE, /* 候选节点 */
} mmdb_replica_role_t;

/* 复制状态信息 */
typedef struct {
    mmdb_replica_role_t  role;           /* 当前角色 */
    const char*          leader_addr;    /* 主节点地址（仅 follower） */
    uint64_t             commit_index;   /* 已提交日志索引 */
    uint64_t             applied_index;  /* 已应用日志索引 */
    bool                 is_synced;      /* 是否已同步 */
    uint32_t             term;           /* 当前 Raft 任期 */
    uint32_t             node_id;        /* 本节点 ID */
} mmdb_replica_info_t;

/* 复制句柄（前向声明） */
typedef struct mmdb_replica_s mmdb_replica_t;

/**
 * @brief 初始化复制模式
 * @param db    数据库句柄
 * @param role  节点角色
 * @param peers JSON 格式的集群节点列表，格式：[{"id":1,"addr":"host:port"},...]
 * @return MMDB_OK 成功，其他表示错误
 */
int mmdb_replication_init(mmdb_t* db, mmdb_replica_role_t role, const char* peers);

/**
 * @brief 获取复制状态
 * @param db   数据库句柄
 * @param info 输出的复制状态信息
 * @return MMDB_OK 成功
 */
int mmdb_replication_info(mmdb_t* db, mmdb_replica_info_t* info);

/**
 * @brief 触发故障转移（仅 follower 调用）
 * @param db 数据库句柄
 * @return MMDB_OK 成功发起选举
 */
int mmdb_replication_failover(mmdb_t* db);

/**
 * @brief 停止复制
 * @param db 数据库句柄
 * @return MMDB_OK 成功
 */
int mmdb_replication_stop(mmdb_t* db);

/**
 * @brief 以 Leader 身份写入日志（内部使用）
 * @param db      数据库句柄
 * @param data    日志数据
 * @param len     数据长度
 * @return MMDB_OK 成功
 */
int mmdb_replication_append_log(mmdb_t* db, const void* data, uint32_t len);

/**
 * @brief 以 Follower 身份应用日志（内部使用）
 * @param db      数据库句柄
 * @param data    日志数据
 * @param len     数据长度
 * @return MMDB_OK 成功
 */
int mmdb_replication_apply_log(mmdb_t* db, const void* data, uint32_t len);

#ifdef __cplusplus
}
#endif

#endif /* MMDB_REPLICATION_H */
