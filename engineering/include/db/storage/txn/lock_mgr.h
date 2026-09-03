/**
 * @file lock_mgr.h
 * @brief 锁管理器头文件
 *
 * 实现 PostgreSQL 风格的锁管理器：
 * - LockTag: 锁的唯一标识
 * - 锁类型: SHARED/EXCLUSIVE/ACCESS SHARE/等
 * - 锁粒度: RELATION/PAGE/TUPLE/TRANSACTION
 * - 死锁检测
 * - 锁超时
 *
 * 参考 PostgreSQL: src/backend/storage/lmgr/lock.c
 */
#ifndef DB_LOCK_MGR_H
#define DB_LOCK_MGR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 锁粒度
 * ======================================================================== */

/** 锁粒度 */
typedef enum LockTagType_e {
    LOCKTAG_RELATION = 0,    /**< 表级锁 */
    LOCKTAG_PAGE = 1,       /**< 页面级锁 */
    LOCKTAG_TUPLE = 2,      /**< 行级锁 */
    LOCKTAG_TRANSACTION = 3, /**< 事务锁 */
    LOCKTAG_OBJECT = 4,     /**< 对象锁 */
    LOCKTAG_predicate = 5   /**< 谓词锁 */
} LockTagType;

/* ========================================================================
 * LockTag 结构
 * ======================================================================== */

/**
 * @brief 锁标签
 *
 * 唯一标识一个需要加锁的资源
 *
 * 结构设计（16字节）：
 * ┌─────────────────────────────────────────────────────┐
 * │ locktag_type (1) │ locktag_lockmethodid (1)         │
 * │ locktag_field1 (4) - relfilenode                   │
 * │ locktag_field2 (4) - blocknum (或 OID)             │
 * │ locktag_field3 (4) - offset (或 subid)            │
 * │ locktag_field4 (4) - 扩展字段                      │
 * └─────────────────────────────────────────────────────┘
 */
typedef struct LockTag_s {
    uint8_t  locktag_type;         /**< 锁粒度类型 */
    uint8_t  locktag_lockmethodid;  /**< 锁方法 ID */
    uint32_t locktag_field1;       /**< 字段1：通常为 relfilenode */
    uint32_t locktag_field2;       /**< 字段2：页面号/OID */
    uint32_t locktag_field3;       /**< 字段3：元组偏移/子ID */
    uint32_t locktag_field4;       /**< 字段4：扩展字段 */
} LockTag;

/** LockTag 哈希值类型 */
typedef uint64_t LOCKTAG_HASH;

/* ========================================================================
 * 锁类型
 * ======================================================================== */

/** 锁模式 */
typedef enum LockMode_e {
    NoLock = 0,              /**< 无锁（仅用于内部） */
    AccessShareLock = 1,     /**< ACCESS SHARE - SELECT */
    RowShareLock = 2,        /**< ROW SHARE - SELECT FOR UPDATE/FOR SHARE */
    RowExclusiveLock = 3,    /**< ROW EXCLUSIVE - INSERT/UPDATE/DELETE */
    ShareUpdateExclusiveLock = 4, /**< SHARE UPDATE EXCLUSIVE - VACUUM/ANALYZE */
    ShareLock = 5,           /**< SHARE - CREATE INDEX */
    ShareRowExclusiveLock = 6,   /**< SHARE ROW EXCLUSIVE - ALTER TABLE */
    ExclusiveLock = 7,       /**< EXCLUSIVE - 排斥大多数操作 */
    AccessExclusiveLock = 8   /**< ACCESS EXCLUSIVE - DROP/TRUNCATE */
} LockMode;

/** 锁兼容矩阵索引 */
#define LOCKMODE_NUM 9

/* ========================================================================
 * 锁条目
 * ======================================================================== */

/** 锁持有者信息 */
typedef struct LockMember_s {
    TransactionId xid;       /**< 持有事务 ID */
    uint32_t pid;           /**< 进程 ID */
    LockMode mode;          /**< 锁模式 */
    bool granted;           /**< 是否已授予 */
} LockMember;

/** 锁条目 */
typedef struct LockEntry_s {
    LockTag tag;                 /**< 锁标签 */
    LockMember holder;            /**< 当前持有者 */
    int wait_queue_count;        /**< 等待队列长度 */
    int max_waiters;             /**< 最大等待者数 */
    struct LockEntry_s *next;    /**< 链表下一个 */
} LockEntry;

/** 锁等待者 */
typedef struct LockWaiter_s {
    TransactionId xid;       /**< 等待事务 ID */
    uint32_t pid;           /**< 进程 ID */
    LockMode mode;          /**< 请求的锁模式 */
    bool waiting;           /**< 是否在等待 */
    struct LockWaiter_s *next; /**< 链表下一个 */
} LockWaiter;

/* ========================================================================
 * 死锁检测
 * ======================================================================== */

/** Wait-For Graph 节点 */
typedef struct WfgNode_s {
    TransactionId xid;           /**< 事务 ID */
    struct WfgEdge_s *out_edges; /**< 出边链表 */
    int in_degree;              /**< 入度 */
} WfgNode;

/** Wait-For Graph 边 */
typedef struct WfgEdge_s {
    TransactionId waiter;    /**< 等待者 */
    TransactionId holder;    /**< 持有者 */
    LockTag tag;             /**< 锁标签 */
    struct WfgEdge_s *next;  /**< 链表下一个 */
} WfgEdge;

/* ========================================================================
 * GUC 参数
 * ======================================================================== */

/** 锁超时（毫秒） */
extern uint32_t g_lock_timeout;

/** 死锁检测超时（毫秒） */
extern uint32_t g_deadlock_timeout;

/** 默认锁超时（30秒） */
#define DEFAULT_LOCK_TIMEOUT 30000

/** 默认死锁检测超时（1秒） */
#define DEFAULT_DEADLOCK_TIMEOUT 1000

/* ========================================================================
 * 锁操作 API
 * ======================================================================== */

/**
 * @brief 初始化锁管理器
 */
void lock_mgr_init(void);

/**
 * @brief 关闭锁管理器
 */
void lock_mgr_shutdown(void);

/**
 * @brief 获取锁
 *
 * @param tag 锁标签
 * @param xid 事务 ID
 * @param mode 锁模式
 * @param progress 是否报告进度
 * @return true 成功获取锁
 */
bool lock_acquire(const LockTag *tag, TransactionId xid, LockMode mode, bool progress);

/**
 * @brief 释放锁
 *
 * @param tag 锁标签
 * @param xid 事务 ID
 * @param mode 锁模式
 * @return true 成功释放
 */
bool lock_release(const LockTag *tag, TransactionId xid, LockMode mode);

/**
 * @brief 尝试获取锁（非阻塞）
 *
 * @param tag 锁标签
 * @param xid 事务 ID
 * @param mode 锁模式
 * @return true 成功获取，false 被阻塞
 */
bool lock_acquire_nowait(const LockTag *tag, TransactionId xid, LockMode mode);

/**
 * @brief 检查锁是否被持有
 *
 * @param tag 锁标签
 * @param mode 锁模式
 * @return true 被持有
 */
bool lock_is_held(const LockTag *tag, LockMode mode);

/**
 * @brief 检查锁是否被指定事务持有
 */
bool lock_is_held_by_xid(const LockTag *tag, TransactionId xid, LockMode mode);

/**
 * @brief 释放事务的所有锁
 *
 * @param xid 事务 ID
 * @return 释放的锁数量
 */
int lock_release_all_for_xid(TransactionId xid);

/**
 * @brief 更新锁超时
 *
 * @param timeout_ms 超时（毫秒）
 */
void lock_set_timeout(uint32_t timeout_ms);

/* ========================================================================
 * 死锁检测
 * ======================================================================== */

/**
 * @brief 执行死锁检测
 *
 * 通过 Wait-For Graph 检测循环等待
 *
 * @return true 发现死锁
 */
bool deadlock_check(void);

/**
 * @brief 获取死锁中被选择回滚的事务
 *
 * @return 死锁中选择回滚的事务 ID
 */
TransactionId deadlock_select_victim(void);

/**
 * @brief 清理死锁图
 */
void deadlock_graph_clear(void);

/* ========================================================================
 * 锁等待超时处理
 * ======================================================================== */

/**
 * @brief 检查锁等待是否超时
 *
 * @param waiter 等待者
 * @return true 超时
 */
bool lock_wait_timeout(const LockWaiter *waiter);

/**
 * @brief 获取锁等待的等待时间
 *
 * @param waiter 等待者
 * @return 等待时间（毫秒）
 */
uint64_t lock_wait_elapsed(const LockWaiter *waiter);

/* ========================================================================
 * 锁信息查询
 * ======================================================================== */

/**
 * @brief 获取指定标签的锁信息
 *
 * @param tag 锁标签
 * @return 锁条目
 */
LockEntry *lock_get_entry(const LockTag *tag);

/**
 * @brief 获取事务持有的锁数量
 *
 * @param xid 事务 ID
 * @return 锁数量
 */
int lock_count_for_xid(TransactionId xid);

/**
 * @brief 检查两个锁模式是否兼容
 *
 * @param held 已持有的模式
 * @param requested 请求的模式
 * @return true 兼容
 */
bool lock_mode_compatible(LockMode held, LockMode requested);

/**
 * @brief 获取锁模式的锁级别
 *
 * 锁级别用于判断优先级
 *
 * @param mode 锁模式
 * @return 锁级别（数值越大优先级越高）
 */
int lock_mode_to_locklevel(LockMode mode);

/* ========================================================================
 * 锁统计
 * ======================================================================== */

/** 锁统计信息 */
typedef struct LockStats_s {
    uint64_t lock_acquires;      /**< 锁获取次数 */
    uint64_t lock_releases;      /**< 锁释放次数 */
    uint64_t lock_waits;        /**< 锁等待次数 */
    uint64_t lock_deadlocks;     /**< 死锁次数 */
    uint64_t lock_timeouts;      /**< 超时次数 */
} LockStats;

/**
 * @brief 获取锁统计
 */
void lock_get_stats(LockStats *stats);

/**
 * @brief 重置锁统计
 */
void lock_reset_stats(void);

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印锁状态
 */
void lock_dump(void);

/**
 * @brief 打印死锁图
 */
void deadlock_graph_dump(void);

#ifdef __cplusplus
}
#endif

#endif /* DB_LOCK_MGR_H */
