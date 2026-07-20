/**
 * @file mvcc_wal.h
 * @brief MVCC 与 WAL 集成头文件
 *
 * 实现 MVCC 与 WAL 的深度集成：
 * 1. 在 WAL 记录中包含 MVCC 元组版本信息
 * 2. 事务提交时记录提交序列号（CSN）
 * 3. 恢复时使用 MVCC 信息进行一致性验证
 *
 * 参考 PostgreSQL: src/backend/access/transam/xlogutils.c
 */
#ifndef DB_MVCC_WAL_H
#define DB_MVCC_WAL_H

#include "db/storage/txn/mvcc.h"
#include "db/storage/wal/wal.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * MVCC WAL 记录类型
 * ======================================================================== */

/** MVCC WAL 记录类型（扩展标准 WAL 类型） */
typedef enum mvcc_wal_type_e {
    MVCC_WAL_HEAP_INSERT = 100,    /**< 堆表插入 */
    MVCC_WAL_HEAP_UPDATE = 101,   /**< 堆表更新 */
    MVCC_WAL_HEAP_DELETE = 102,   /**< 堆表删除 */
    MVCC_WAL_HEAP_LOCK = 103,     /**< 堆表锁 */
    MVCC_WAL_TUPLE_FREEZE = 104,  /**< 元组冻结 */
    MVCC_WAL_XACT_COMMIT = 105,   /**< 事务提交（含 CSN） */
    MVCC_WAL_XACT_ABORT = 106,    /**< 事务回滚 */
    MVCC_WAL_VACUUM = 107         /**< VACUUM 操作 */
} mvcc_wal_type_t;

/* ========================================================================
 * MVCC WAL 记录头
 * ======================================================================== */

#pragma pack(push, 1)

/**
 * @brief MVCC WAL 记录头
 *
 * 追加在标准 WAL 记录头之后
 */
typedef struct MvccWalHeader_s {
    TransactionId xmin;        /**< 插入事务 ID */
    TransactionId xmax;        /**< 删除/更新事务 ID */
    uint32_t t_ctid;         /**< 元组指针（block << 16 | offset） */
    uint8_t  xmin_status;    /**< xmin 状态 */
    uint8_t  xmax_status;    /**< xmax 状态 */
    uint8_t  info;           /**< 扩展信息位 */
} MvccWalHeader;

#pragma pack(pop)

/** MVCC WAL 记录头大小 */
#define MVCC_WAL_HEADER_SIZE 16

/* ========================================================================
 * 事务提交序列号（CSN）
 * ======================================================================== */

/**
 * @brief 提交序列号记录
 *
 * 用于 MVCC 的可见性判断，替代 xid 顺序
 */
typedef struct XactCommitSeqno_s {
    TransactionId xid;         /**< 事务 ID */
    uint64_t csn;            /**< 提交序列号 */
    uint64_t commit_time;    /**< 提交时间戳 */
} XactCommitSeqno;

/* ========================================================================
 * MVCC WAL 接口
 * ======================================================================== */

/**
 * @brief 初始化 MVCC WAL 集成
 */
void mvcc_wal_init(void);

/**
 * @brief 关闭 MVCC WAL 集成
 */
void mvcc_wal_shutdown(void);

/**
 * @brief 写入 MVCC 堆表插入 WAL
 *
 * @param wal WAL 句柄
 * @param txn_id 事务 ID
 * @param relfilenode 关系文件节点
 * @param blocknum 页面号
 * @param offset 元组偏移
 * @param xmin 插入事务 ID
 * @param data 元组数据
 * @param data_len 数据长度
 * @return LSN
 */
uint64_t mvcc_wal_heap_insert(wal_t *wal,
                              uint32_t txn_id,
                              uint32_t relfilenode,
                              uint32_t blocknum,
                              uint32_t offset,
                              TransactionId xmin,
                              const void *data,
                              size_t data_len);

/**
 * @brief 写入 MVCC 堆表更新 WAL
 *
 * @param wal WAL 句柄
 * @param txn_id 事务 ID
 * @param relfilenode 关系文件节点
 * @param blocknum 页面号
 * @param offset 元组偏移
 * @param new_offset 新元组偏移
 * @param xmin 原元组 xmin
 * @param xmax 删除事务 ID
 * @param old_data 旧数据（用于 undo）
 * @param old_len 旧数据长度
 * @param new_data 新数据
 * @param new_len 新数据长度
 * @return LSN
 */
uint64_t mvcc_wal_heap_update(wal_t *wal,
                             uint32_t txn_id,
                             uint32_t relfilenode,
                             uint32_t blocknum,
                             uint32_t offset,
                             uint32_t new_blocknum,
                             uint32_t new_offset,
                             TransactionId xmin,
                             TransactionId xmax,
                             const void *old_data,
                             size_t old_len,
                             const void *new_data,
                             size_t new_len);

/**
 * @brief 写入 MVCC 堆表删除 WAL
 *
 * @param wal WAL 句柄
 * @param txn_id 事务 ID
 * @param relfilenode 关系文件节点
 * @param blocknum 页面号
 * @param offset 元组偏移
 * @param xmin 原元组 xmin
 * @param xmax 删除事务 ID
 * @param old_data 旧数据（用于 undo）
 * @param old_len 旧数据长度
 * @return LSN
 */
uint64_t mvcc_wal_heap_delete(wal_t *wal,
                              uint32_t txn_id,
                              uint32_t relfilenode,
                              uint32_t blocknum,
                              uint32_t offset,
                              TransactionId xmin,
                              TransactionId xmax,
                              const void *old_data,
                              size_t old_len);

/**
 * @brief 写入事务提交 WAL（含 CSN）
 *
 * @param wal WAL 句柄
 * @param txn_id 事务 ID
 * @param csn 提交序列号
 * @return LSN
 */
uint64_t mvcc_wal_xact_commit(wal_t *wal, TransactionId txn_id, uint64_t csn);

/**
 * @brief 写入事务回滚 WAL
 *
 * @param wal WAL 句柄
 * @param txn_id 事务 ID
 * @return LSN
 */
uint64_t mvcc_wal_xact_abort(wal_t *wal, TransactionId txn_id);

/**
 * @brief 写入元组冻结 WAL
 *
 * @param wal WAL 句柄
 * @param txn_id 事务 ID
 * @param relfilenode 关系文件节点
 * @param blocknum 页面号
 * @param offset 元组偏移
 * @param old_xmin 旧的 xmin
 * @return LSN
 */
uint64_t mvcc_wal_tuple_freeze(wal_t *wal,
                               TransactionId txn_id,
                               uint32_t relfilenode,
                               uint32_t blocknum,
                               uint32_t offset,
                               TransactionId old_xmin);

/**
 * @brief 写入 VACUUM WAL
 *
 * @param wal WAL 句柄
 * @param txn_id 事务 ID
 * @param relfilenode 关系文件节点
 * @param blocknum 页面号
 * @param offsets 清理的元组偏移数组
 * @param count 清理的元组数量
 * @return LSN
 */
uint64_t mvcc_wal_vacuum(wal_t *wal,
                        TransactionId txn_id,
                        uint32_t relfilenode,
                        uint32_t blocknum,
                        const uint32_t *offsets,
                        size_t count);

/* ========================================================================
 * CSN 管理
 * ======================================================================== */

/** 全局 CSN 计数器（原子操作） */
extern uint64_t g_commit_seqno;

/**
 * @brief 分配新的提交序列号
 * @return 新的 CSN
 */
uint64_t mvcc_alloc_csn(void);

/**
 * @brief 获取 CSN 对应的事务 ID
 *
 * @param csn 提交序列号
 * @return 事务 ID，未知返回 INVALID_TRANSACTION_ID
 */
TransactionId mvcc_csn_to_xid(uint64_t csn);

/**
 * @brief 检查 CSN 是否已提交
 *
 * @param csn 提交序列号
 * @return true 已提交
 */
bool mvcc_csn_committed(uint64_t csn);

/* ========================================================================
 * 恢复时的 MVCC 处理
 * ======================================================================== */

/**
 * @brief 恢复上下文
 */
typedef struct MvccRecoveryCtx_s {
    uint64_t current_csn;              /**< 当前恢复的 CSN */
    TransactionId last_complete_xid;   /**< 上一个已完成的事务 */
} MvccRecoveryCtx;

/**
 * @brief 初始化 MVCC 恢复上下文
 */
MvccRecoveryCtx *mvcc_recovery_init(void);

/**
 * @brief 清理 MVCC 恢复上下文
 */
void mvcc_recovery_cleanup(MvccRecoveryCtx *ctx);

/**
 * @brief 处理 MVCC WAL 记录（恢复时调用）
 *
 * @param ctx 恢复上下文
 * @param type 日志类型
 * @param header MVCC WAL 头
 * @param data 数据
 * @param data_len 数据长度
 * @return 0 成功
 */
int mvcc_recovery_apply(MvccRecoveryCtx *ctx,
                       mvcc_wal_type_t type,
                       const MvccWalHeader *header,
                       const void *data,
                       size_t data_len);

/* ========================================================================
 * 可见性与 CSN
 * ======================================================================== */

/**
 * @brief 使用 CSN 判断元组是否可见
 *
 * @param rv ReadView
 * @param tuple_fields 元组字段
 * @param current_xid 当前事务 ID
 * @param csn 元组的提交序列号（如果有）
 * @return true 可见
 */
bool mvcc_tuple_visible_by_csn(const ReadView *rv,
                               const HeapTupleFields *tuple_fields,
                               TransactionId current_xid,
                               uint64_t csn);

/**
 * @brief 获取元组的提交序列号
 *
 * @param xid 事务 ID
 * @return CSN，未知返回 0
 */
uint64_t mvcc_get_tuple_csn(TransactionId xid);

#ifdef __cplusplus
}
#endif

#endif /* DB_MVCC_WAL_H */
