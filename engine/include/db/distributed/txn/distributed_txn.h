/**
 * @file distributed_txn.h
 * @brief 分布式事务接口（两阶段提交协议）
 *
 * Phase12 - 实现分布式两阶段提交（2PC）协议。
 *
 * 设计目标：
 * - 支持多参与者分布式事务
 * - coordinator 和 participant 角色
 * - Prepare/Commit/Rollback 协议
 * - 故障恢复支持
 */
#ifndef DB_DISTRIBUTED_TXN_H
#define DB_DISTRIBUTED_TXN_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define DISTRIBUTED_TXN_MAX_PARTICIPANTS 64
#define DISTRIBUTED_TXN_MAX_RETRIES 3

/* ========================================================================
 * 错误码定义
 * ======================================================================== */

typedef enum {
    DISTRIBUTED_TXN_OK = 0,
    DISTRIBUTED_TXN_TIMEOUT = -1,
    DISTRIBUTED_TXN_ABORT = -2,
    DISTRIBUTED_TXN_COMMIT = -3,
    DISTRIBUTED_TXN_PENDING = -4,
    DISTRIBUTED_TXN_INVALID_STATE = -5,
    DISTRIBUTED_TXN_NETWORK_ERROR = -6,
    DISTRIBUTED_TXN_PROTOCOL_ERROR = -7,
    DISTRIBUTED_TXN_OUT_OF_MEMORY = -8
} distributed_txn_error_t;

/* ========================================================================
 * 事务状态
 * ======================================================================== */

typedef enum {
    DTXN_STATE_INIT = 0,           /**< 初始化 */
    DTXN_STATE_OPEN = 1,           /**< 打开 */
    DTXN_STATE_PREPARING = 2,      /**< 准备中 */
    DTXN_STATE_PREPARED = 3,       /**< 已准备好 */
    DTXN_STATE_COMMITTING = 4,     /**< 提交中 */
    DTXN_STATE_COMMITTED = 5,      /**< 已提交 */
    DTXN_STATE_ABORTING = 6,       /**< 中止中 */
    DTXN_STATE_ABORTED = 7,        /**< 已中止 */
    DTXN_STATE_UNKNOWN = 8          /**< 未知（参与者超时） */
} distributed_txn_state_t;

/* ========================================================================
 * 参与者状态
 * ======================================================================== */

typedef enum {
    DTXN_PARTICIPANT_INIT = 0,
    DTXN_PARTICIPANT_PREPARED = 1,
    DTXN_PARTICIPANT_COMMITTED = 2,
    DTXN_PARTICIPANT_ABORTED = 3,
    DTXN_PARTICIPANT_UNKNOWN = 4
} distributed_txn_participant_state_t;

/* ========================================================================
 * 数据结构
 * ======================================================================== */

/**
 * @brief 事务参与者信息
 */
typedef struct distributed_txn_participant {
    uint64_t node_id;                              /**< 节点 ID */
    char address[256];                              /**< 节点地址 */
    uint16_t port;                                 /**< 端口 */
    distributed_txn_participant_state_t state;      /**< 参与者状态 */
    bool voted_yes;                                /**< 是否投赞成票 */
    int retry_count;                               /**< 重试次数 */
    uint64_t last_response_time;                    /**< 最后响应时间 */
} distributed_txn_participant_t;

/**
 * @brief 事务上下文
 */
typedef struct distributed_txn distributed_txn_t;

/**
 * @brief 事务操作接口
 */
typedef struct {
    /** 准备事务（锁定资源）*/
    int (*prepare)(void *ctx, distributed_txn_t *txn);

    /** 提交事务（释放资源并提交）*/
    int (*commit)(void *ctx, distributed_txn_t *txn);

    /** 中止事务（回滚）*/
    int (*abort)(void *ctx, distributed_txn_t *txn);

    /** 获取事务 ID */
    const char* (*get_id)(void *ctx, distributed_txn_t *txn);

    /** 用户数据 */
    void *user_data;
} distributed_txn_ops_t;

/**
 * @brief 事务结果回调
 */
typedef void (*distributed_txn_callback_t)(distributed_txn_t *txn,
                                         distributed_txn_state_t final_state,
                                         void *user_data);

/* ========================================================================
 * 协调者接口
 * ======================================================================== */

/**
 * @brief 创建分布式事务协调者
 *
 * @param transaction_id 事务 ID
 * @param ops 事务操作接口
 * @return 成功返回协调者指针，失败返回 NULL
 */
distributed_txn_t *dtxn_coordinator_create(const char *transaction_id,
                                          const distributed_txn_ops_t *ops);

/**
 * @brief 销毁分布式事务协调者
 */
void dtxn_coordinator_destroy(distributed_txn_t *txn);

/**
 * @brief 添加参与者
 *
 * @param txn 事务
 * @param node_id 节点 ID
 * @param address 节点地址
 * @param port 端口
 * @return 0 成功，非 0 失败
 */
int dtxn_add_participant(distributed_txn_t *txn,
                        uint64_t node_id,
                        const char *address,
                        uint16_t port);

/**
 * @brief 开始两阶段提交
 *
 * Phase 1: 向所有参与者发送 Prepare
 * Phase 2a: 如果全部投票 Yes，发送 Commit
 * Phase 2b: 如果有任何投票 No，发送 Abort
 *
 * @param txn 事务
 * @param timeout_ms 超时时间（毫秒）
 * @return 0 提交成功，DISTRIBUTED_TXN_ABORT 需要回滚，其他负值错误
 */
int dtxn_begin_2pc(distributed_txn_t *txn, uint32_t timeout_ms);

/**
 * @brief 异步开始两阶段提交
 *
 * @param txn 事务
 * @param callback 完成回调
 */
void dtxn_begin_2pc_async(distributed_txn_t *txn,
                          distributed_txn_callback_t callback);

/**
 * @brief 中止事务
 *
 * @param txn 事务
 * @return 0 成功
 */
int dtxn_abort(distributed_txn_t *txn);

/**
 * @brief 获取事务状态
 */
distributed_txn_state_t dtxn_get_state(const distributed_txn_t *txn);

/**
 * @brief 获取事务 ID
 */
const char *dtxn_get_id(const distributed_txn_t *txn);

/**
 * @brief 获取事务结果描述
 */
const char *dtxn_error_string(distributed_txn_error_t error);

/* ========================================================================
 * 参与者接口（供节点调用）
 * ======================================================================== */

/**
 * @brief 创建事务参与者
 *
 * @param node_id 节点 ID
 * @param ops 事务操作接口
 * @return 成功返回参与者指针，失败返回 NULL
 */
distributed_txn_t *dtxn_participant_create(uint64_t node_id,
                                          const distributed_txn_ops_t *ops);

/**
 * @brief 处理协调者的 Prepare 请求
 *
 * @param txn 参与者事务
 * @param coordinator_id 协调者 ID
 * @param transaction_id 事务 ID
 * @return true 投赞成票，false 投反对票
 */
bool dtxn_handle_prepare(distributed_txn_t *txn,
                         uint64_t coordinator_id,
                         const char *transaction_id);

/**
 * @brief 处理协调者的 Commit 请求
 *
 * @param txn 参与者事务
 * @return 0 成功
 */
int dtxn_handle_commit(distributed_txn_t *txn);

/**
 * @brief 处理协调者的 Abort 请求
 *
 * @param txn 参与者事务
 * @return 0 成功
 */
int dtxn_handle_abort(distributed_txn_t *txn);

/* ========================================================================
 * 持久化接口（用于故障恢复）
 * ======================================================================== */

/**
 * @brief 保存事务状态到持久化存储
 *
 * @param txn 事务
 * @param path 文件路径
 * @return 0 成功
 */
int dtxn_persist_state(const distributed_txn_t *txn, const char *path);

/**
 * @brief 从持久化存储恢复事务状态
 *
 * @param path 文件路径
 * @param ops 事务操作接口
 * @return 成功返回恢复的事务，失败返回 NULL
 */
distributed_txn_t *dtxn_recover_state(const char *path,
                                     const distributed_txn_ops_t *ops);

/* ========================================================================
 * 超时检测
 * ======================================================================== */

/**
 * @brief 检查参与者超时
 *
 * @param txn 事务
 * @param timeout_ms 超时阈值
 * @return true 有参与者超时
 */
bool dtxn_check_participant_timeout(const distributed_txn_t *txn,
                                   uint32_t timeout_ms);

/**
 * @brief 清理超时参与者
 *
 * @param txn 事务
 * @param timeout_ms 超时阈值
 * @return 被清理的参与者数量
 */
int dtxn_cleanup_timedout_participants(distributed_txn_t *txn,
                                       uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* DB_DISTRIBUTED_TXN_H */
