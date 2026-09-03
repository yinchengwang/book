/**
 * @file predicate.h
 * @brief 谓词锁和 SSI 检测头文件
 *
 * 实现 PostgreSQL 风格的 Serializable Snapshot Isolation (SSI)：
 * - 谓词锁（Predicate Lock）
 * - SIREAD 锁集合
 * - 序列化异常检测
 *
 * 参考：src/backend/storage/lmgr/predicate.c
 */
#ifndef DB_PREDICATE_H
#define DB_PREDICATE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 谓词锁类型
 * ======================================================================== */

/** 谓词锁类型 */
typedef enum {
    PREDICATE_LOCK_SHARE = 0,      /**< 共享锁（读） */
    PREDICATE_LOCK_EXCLUSIVE = 1   /**< 排他锁（写） */
} predicate_lock_type_t;

/** 锁粒度 */
typedef enum {
    PREDICATE_LOCK_RELATION = 0,  /**< 表级锁 */
    PREDICATE_LOCK_PAGE = 1,       /**< 页面级锁 */
    PREDICATE_LOCK_TUPLE = 2,     /**< 元组级锁 */
    PREDICATE_LOCK_KEYRANGE = 3   /**< 键范围锁 */
} predicate_lock_target_t;

/* ========================================================================
 * 谓词锁结构
 * ======================================================================== */

/**
 * @brief 谓词锁标签
 */
typedef struct PredicateLockTag_s {
    uint32_t relfilenode;         /**< 关系文件节点 */
    uint32_t blocknum;            /**< 页面号（0 表示表级） */
    uint32_t offset;              /**< 元组偏移（0 表示页面级） */
    uint64_t key_hash;            /**< 键哈希（用于范围锁） */
    predicate_lock_target_t target; /**< 锁粒度 */
} PredicateLockTag;

/**
 * @brief 谓词锁条目
 */
typedef struct PredicateLockEntry_s {
    PredicateLockTag tag;              /**< 锁标签 */
    TransactionId holder_xid;          /**< 持有事务 ID */
    predicate_lock_type_t lock_type;   /**< 锁类型 */
    bool committed;                    /**< 持有事务是否已提交 */
} PredicateLockEntry;

/**
 * @brief SIREAD 锁条目（表级共享读锁）
 */
typedef struct SireadLockEntry_s {
    uint32_t relfilenode;         /**< 关系文件节点 */
    TransactionId xid;            /**< 持有事务 ID */
} SireadLockEntry;

/* ========================================================================
 * 序列化异常
 * ======================================================================== */

/** 序列化异常类型 */
typedef enum {
    SERIALIZE_CONFLICT_NONE = 0,
    SERIALIZE_CONFLICT_RW = 1,   /**< 读写冲突 */
    SERIALIZE_CONFLICT_WR = 2,   /**< 写读冲突 */
    SERIALIZE_CONFLICT_WW = 3    /**< 写写冲突 */
} serialize_conflict_type_t;

/**
 * @brief 序列化异常信息
 */
typedef struct SerializeConflict_s {
    TransactionId xid1;                      /**< 事务 1 */
    TransactionId xid2;                      /**< 事务 2 */
    serialize_conflict_type_t type;          /**< 冲突类型 */
    PredicateLockTag tag;                    /**< 冲突的锁标签 */
} SerializeConflict;

/* ========================================================================
 * SSI 状态跟踪
 * ======================================================================== */

/** SSI 状态结构 */
typedef struct SsiState_s {
    /** 读写依赖：T1 读 A，T2 写 A -> T2 依赖于 T1 */
    TransactionId rw_depend[256];
    int rw_depend_count;

    /** 写读依赖：T1 写 A，T2 读 A -> T2 依赖于 T1 */
    TransactionId wr_depend[256];
    int wr_depend_count;

    /** 序列化异常记录 */
    SerializeConflict conflicts[32];
    int conflict_count;

    /** 已检测到的序列化异常 */
    bool serialize_failure;
} SsiState;

/* ========================================================================
 * 谓词锁管理
 * ======================================================================== */

/**
 * @brief 初始化谓词锁子系统
 */
void predicate_lock_init(void);

/**
 * @brief 清理谓词锁子系统
 */
void predicate_lock_shutdown(void);

/**
 * @brief 获取表级 SIREAD 锁
 *
 * 当事务读取表中的任何元组时，需要获取 SIREAD 锁
 */
bool predicate_lock_relation(uint32_t relfilenode, TransactionId xid);

/**
 * @brief 获取页面级谓词锁
 */
bool predicate_lock_page(uint32_t relfilenode, uint32_t blocknum,
                         TransactionId xid, predicate_lock_type_t lock_type);

/**
 * @brief 获取元组级谓词锁
 */
bool predicate_lock_tuple(uint32_t relfilenode, uint32_t blocknum,
                          uint32_t offset, TransactionId xid,
                          predicate_lock_type_t lock_type);

/**
 * @brief 获取键范围谓词锁
 */
bool predicate_lock_keyrange(uint32_t relfilenode, uint64_t key_hash,
                             TransactionId xid, predicate_lock_type_t lock_type);

/**
 * @brief 检查是否存在排他锁冲突
 *
 * @param tag 锁标签
 * @param xid 检查冲突的事务 ID
 * @return true 存在冲突
 */
bool predicate_has_exclusive_conflict(const PredicateLockTag *tag,
                                      TransactionId xid);

/**
 * @brief 检查是否存在共享锁冲突
 */
bool predicate_has_shared_conflict(const PredicateLockTag *tag,
                                   TransactionId xid);

/* ========================================================================
 * SSI 检测
 * ======================================================================== */

/**
 * @brief 记录读写依赖
 *
 * 事务 T2 读取了事务 T1 写入的数据
 * 格式：T1 -rw-> T2
 */
void ssi_record_rw_depend(TransactionId writer_xid, TransactionId reader_xid);

/**
 * @brief 记录写读依赖
 *
 * 事务 T2 写入了事务 T1 读取的数据
 * 格式：T1 -wr-> T2
 */
void ssi_record_wr_depend(TransactionId reader_xid, TransactionId writer_xid);

/**
 * @brief 检测序列化异常
 *
 * 当存在：T1 -rw-> T2 且 T2 -wr-> T1 时，发生序列化异常
 */
bool ssi_detect_anomaly(void);

/**
 * @brief 获取 SSI 状态
 */
SsiState *ssi_get_state(void);

/**
 * @brief 重置 SSI 状态
 */
void ssi_reset(void);

/**
 * @brief 检查当前事务是否可以继续
 *
 * @return true 可以继续，false 发生序列化冲突需要重试
 */
bool ssi_can_continue(void);

/**
 * @brief 标记序列化失败
 */
void ssi_mark_failure(void);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印谓词锁状态
 */
void predicate_lock_dump(void);

/**
 * @brief 打印 SSI 状态
 */
void ssi_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_PREDICATE_H */
