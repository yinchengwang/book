/**
 * @file lock.h
 * @brief 数据库锁管理器公共接口
 *
 * 支持多粒度锁：表锁、页锁、行锁
 * 支持意向锁：IS/IX/SIX
 * 支持死锁检测和乐观锁
 */
#ifndef DB_LOCK_H
#define DB_LOCK_H

#include <stdint.h>
#include <stdbool.h>

/* 原子操作兼容性处理 */
#if defined(__cplusplus)
    #include <atomic>
    typedef std::atomic<int> atomic_int_t;
    typedef std::atomic<unsigned int> atomic_uint_t;
    typedef std::atomic<unsigned long> atomic_ulong_t;
    typedef std::atomic<uint64_t> atomic_uint64_t;
    #define ATOMIC_VAR_INIT(v) std::atomic<int>(v)
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #include <stdatomic.h>
    typedef _Atomic int atomic_int_t;
    typedef _Atomic unsigned int atomic_uint_t;
    typedef _Atomic unsigned long atomic_ulong_t;
    typedef _Atomic uint64_t atomic_uint64_t;
#else
    /* 回退：使用 volatile */
    typedef volatile int atomic_int_t;
    typedef volatile unsigned int atomic_uint_t;
    typedef volatile unsigned long atomic_ulong_t;
    typedef volatile uint64_t atomic_uint64_t;
    #define atomic_load_acq(ptr) (*(ptr))
    #define atomic_store_rel(ptr, val) (*(ptr) = (val))
    #define atomic_compare_exchange_strong(ptr, expected, desired) \
        (*(expected) == *(ptr) ? (*(ptr) = (desired), true) : (false))
    #define atomic_fetch_add(ptr, val) ((*(ptr))++)
    #define atomic_init(ptr, val) (*(ptr) = (val))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 锁层级定义
 * ============================================================ */

/** 锁对象类型 */
typedef enum lock_object_type_e {
    LOCK_DATABASE = 0,   /**< 数据库级别锁 */
    LOCK_TABLE,          /**< 表锁 */
    LOCK_PAGE,           /**< 页锁 */
    LOCK_ROW             /**< 行锁 */
} lock_object_type_t;

/** 锁模式 */
typedef enum lock_mode_e {
    LOCK_MODE_S = 0,     /**< 共享锁 (Shared) - 读操作 */
    LOCK_MODE_X,        /**< 排他锁 (Exclusive) - 写操作 */
    LOCK_MODE_IS,        /**< 意向共享锁 (Intention Shared) */
    LOCK_MODE_IX,        /**< 意向排他锁 (Intention Exclusive) */
    LOCK_MODE_SIX        /**< 共享意向排他锁 (Shared Intention Exclusive) */
} lock_mode_t;

/** 锁结果码 */
typedef enum lock_result_e {
    LOCK_OK = 0,                 /**< 成功 */
    LOCK_TIMEOUT = 1,            /**< 锁等待超时 */
    LOCK_DEADLOCK = 2,           /**< 检测到死锁 */
    LOCK_HIERARCHY_VIOLATION = 3, /**< 层级规则违反 */
    LOCK_ERROR = -1              /**< 一般错误 */
} lock_result_t;

/** 锁升级策略 */
typedef enum lock_escalation_policy_e {
    LOCK_ESCALATION_NONE = 0,    /**< 禁用锁升级 */
    LOCK_ESCALATION_AUTO = 1,    /**< 自动升级 */
    LOCK_ESCALATION_MANUAL = 2   /**< 手动升级 */
} lock_escalation_policy_t;

/* ============================================================
 * 锁哈希表大小
 * ============================================================ */

/** 锁哈希表桶数量 */
#define LOCK_HASH_SIZE 1024

/** 锁升级阈值 */
#define ROW_LOCK_ESCALATION_THRESHOLD 50   /**< 行锁数量超过此值时尝试升级 */
#define PAGE_LOCK_ESCALATION_THRESHOLD 10  /**< 页锁数量超过此值时尝试升级 */

/** 默认锁超时 (毫秒) */
#define LOCK_DEFAULT_TIMEOUT_MS 5000

/* ============================================================
 * 前向声明
 * ============================================================ */

typedef struct txn_s txn_t;
typedef struct lock_entry_s lock_entry_t;
typedef struct lock_request_s lock_request_t;
typedef struct lock_manager_s lock_manager_t;
typedef struct lock_ctx_s lock_ctx_t;

/* ============================================================
 * 锁请求结构
 * ============================================================ */

/** 锁请求（等待队列中的项） */
struct lock_request_s {
    txn_t          *txn;           /**< 请求的事务 */
    lock_mode_t    mode;           /**< 请求的锁模式 */
    bool           granted;        /**< 是否已授予 */
    uint64_t       wait_start_time;/**< 等待开始时间 */
    int            recursion;      /**< 重入计数（用于共享锁重入） */
    struct lock_request_s *next;  /**< 队列下一项 */
    struct lock_request_s *prev;  /**< 队列上一项 */
};

/* ============================================================
 * 锁条目结构
 * ============================================================ */

/** 锁条目（锁哈希表的条目） */
struct lock_entry_s {
    lock_object_type_t obj_type;    /**< 对象类型 */
    uint64_t          object_id;   /**< 对象ID（表名哈希/页ID/行键） */
    uint64_t          table_id;    /**< 所属表ID（用于层级验证） */
    lock_mode_t       granted_mode; /**< 已授予的最高级别锁 */
    txn_t             *exclusive_holder; /**< 排他锁持有者 */
    lock_request_t    *shared_holders;   /**< 共享锁持有者链表 */
    lock_request_t    *wait_queue;      /**< 等待队列（FIFO） */
    atomic_int_t      ref_count;        /**< 引用计数 */

    /* 哈希表链表 */
    lock_entry_t      *hash_next;
    lock_entry_t      **hash_prev_ptr;

    /* 锁升级统计 */
    atomic_int_t      row_lock_count;    /**< 该条目下的行锁数量 */

    /* 重入锁支持：记录每个事务的重入次数 */
    atomic_int_t      recursion_count;   /**< 当前持有者的重入计数（排他锁） */
};

/* ============================================================
 * 锁上下文（与事务绑定）
 * ============================================================ */

/** 事务持有的单个锁记录 */
typedef struct lock_record_s {
    lock_object_type_t obj_type;    /**< 对象类型 */
    uint64_t          object_id;   /**< 对象ID */
    lock_mode_t       mode;         /**< 锁模式 */
    struct lock_record_s *next;    /**< 链表下一项 */
} lock_record_t;

/** 锁上下文（每个事务拥有一个） */
struct lock_ctx_s {
    lock_manager_t        *manager;      /**< 锁管理器 */
    txn_t                 *txn;         /**< 关联的事务 */

    /* 事务持有的锁链表（用于快速释放和重入检测） */
    lock_record_t         *held_locks;

    /* 死锁检测用 */
    uint32_t              wait_depth;    /**< 等待深度 */
    bool                  marked;        /**< DFS 标记 */
    uint64_t              wait_start;    /**< 等待开始时间 */
};

/* ============================================================
 * 锁管理器
 * ============================================================ */

/** 锁管理器 */
struct lock_manager_s {
    lock_entry_t          *lock_table[LOCK_HASH_SIZE]; /**< 锁哈希表 */

    /* 配置 */
    bool                  deadlock_detection_enabled; /**< 是否启用死锁检测 */
    uint32_t              deadlock_timeout_ms;         /**< 死锁超时 */
    lock_escalation_policy_t escalation_policy;       /**< 锁升级策略 */

    /* 统计信息 */
    atomic_uint_t          lock_requests;  /**< 总请求数 */
    atomic_uint_t          lock_waits;     /**< 等待次数 */
    atomic_uint_t          deadlocks;      /**< 死锁次数 */
    atomic_uint_t          lock_escalations;/**< 锁升级次数 */
};

/* ============================================================
 * 锁统计信息
 * ============================================================ */

/** 锁统计信息 */
typedef struct lock_stats_s {
    uint64_t  lock_requests;     /**< 总请求数 */
    uint64_t  lock_waits;         /**< 等待次数 */
    uint64_t  deadlocks;          /**< 死锁次数 */
    uint64_t  lock_escalations;   /**< 锁升级次数 */
    uint64_t  active_locks;       /**< 当前活动锁数 */
} lock_stats_t;

/* ============================================================
 * 锁管理器生命周期
 * ============================================================ */

/**
 * @brief 创建锁管理器
 * @return 锁管理器句柄，失败返回 NULL
 */
lock_manager_t *lock_mgr_create(void);

/**
 * @brief 销毁锁管理器
 * @param mgr 锁管理器
 *
 * @note 会断言所有锁已释放
 */
void lock_mgr_destroy(lock_manager_t *mgr);

/* ============================================================
 * 锁获取与释放
 * ============================================================ */

/**
 * @brief 获取锁
 * @param mgr 锁管理器
 * @param txn 事务
 * @param obj_type 对象类型
 * @param object_id 对象ID
 * @param table_id 表ID（用于层级验证）
 * @param mode 锁模式
 * @param timeout_ms 超时时间（毫秒，0 表示无限等待）
 * @return LOCK_OK 成功，LOCK_TIMEOUT 超时，LOCK_DEADLOCK 死锁
 */
lock_result_t lock_acquire(lock_manager_t *mgr, txn_t *txn,
                            lock_object_type_t obj_type, uint64_t object_id,
                            uint64_t table_id, lock_mode_t mode,
                            uint32_t timeout_ms);

/**
 * @brief 释放锁
 * @param mgr 锁管理器
 * @param txn 事务
 * @param obj_type 对象类型
 * @param object_id 对象ID
 * @param mode 锁模式
 */
void lock_release(lock_manager_t *mgr, txn_t *txn,
                 lock_object_type_t obj_type, uint64_t object_id,
                 lock_mode_t mode);

/**
 * @brief 释放事务持有的所有锁
 * @param mgr 锁管理器
 * @param txn 事务
 */
void lock_release_all(lock_manager_t *mgr, txn_t *txn);

/**
 * @brief 检查事务是否持有指定锁
 * @param mgr 锁管理器
 * @param txn 事务
 * @param obj_type 对象类型
 * @param object_id 对象ID
 * @return true 持有，false 未持有
 */
bool lock_held(lock_manager_t *mgr, txn_t *txn,
               lock_object_type_t obj_type, uint64_t object_id);

/**
 * @brief 检查事务是否已经持有该锁（用于可重入检测）
 * @param mgr 锁管理器
 * @param txn 事务
 * @param obj_type 对象类型
 * @param object_id 对象ID
 * @return true 已持有（可重入），false 未持有
 */
bool lock_already_held(lock_manager_t *mgr, txn_t *txn,
                      lock_object_type_t obj_type, uint64_t object_id);

/* ============================================================
 * 锁升级
 * ============================================================ */

/**
 * @brief 尝试将行锁升级为页锁
 * @param mgr 锁管理器
 * @param txn 事务
 * @param table_id 表ID
 * @param page_id 页ID
 * @return true 升级成功，false 升级失败
 */
bool lock_escalate_row_to_page(lock_manager_t *mgr, txn_t *txn,
                               uint64_t table_id, uint64_t page_id);

/**
 * @brief 尝试将页锁升级为表锁
 * @param mgr 锁管理器
 * @param txn 事务
 * @param table_id 表ID
 * @return true 升级成功，false 升级失败
 */
bool lock_escalate_page_to_table(lock_manager_t *mgr, txn_t *txn,
                                 uint64_t table_id);

/**
 * @brief 锁降级（表锁降级为页锁/行锁）
 * @param mgr 锁管理器
 * @param txn 事务
 * @param obj_type 对象类型
 * @param object_id 对象ID
 * @param mode 降级后的锁模式
 * @return LOCK_OK 成功
 */
lock_result_t lock_deescalate(lock_manager_t *mgr, txn_t *txn,
                               lock_object_type_t obj_type, uint64_t object_id,
                               lock_mode_t mode);

/* ============================================================
 * 死锁检测
 * ============================================================ */

/**
 * @brief 手动触发死锁检测
 * @param mgr 锁管理器
 * @return 检测到死锁返回 true
 */
bool lock_detect_deadlock(lock_manager_t *mgr);

/**
 * @brief 检测死锁并返回 victim 事务
 * @param mgr 锁管理器
 * @return victim 事务指针，由调用者负责回滚；无死锁返回 NULL
 */
txn_t *lock_detect_and_resolve_deadlock(lock_manager_t *mgr);

/**
 * @brief 获取死锁环中的事务列表
 * @param mgr 锁管理器
 * @param out_txns 输出数组
 * @param max_count 最大数量
 * @return 实际返回的事务数量
 */
int lock_get_deadlock_cycle(lock_manager_t *mgr, txn_t **out_txns, int max_count);

/**
 * @brief 获取死锁统计
 * @param mgr 锁管理器
 * @return 累计死锁次数
 */
uint64_t lock_get_deadlock_count(lock_manager_t *mgr);

/* ============================================================
 * 锁统计
 * ============================================================ */

/**
 * @brief 获取锁统计信息
 * @param mgr 锁管理器
 * @param stats 输出统计信息
 */
void lock_get_stats(lock_manager_t *mgr, lock_stats_t *stats);

/* ============================================================
 * 锁兼容矩阵
 * ============================================================ */

/**
 * @brief 检查锁兼容性
 * @param granted_mode 已授予的锁模式
 * @param requested_mode 请求的锁模式
 * @return true 兼容，false 不兼容
 */
bool lock_compatible(lock_mode_t granted_mode, lock_mode_t requested_mode);

/**
 * @brief 获取锁模式名称
 * @param mode 锁模式
 * @return 锁模式名称字符串
 */
const char *lock_mode_name(lock_mode_t mode);

/**
 * @brief 获取锁对象类型名称
 * @param obj_type 对象类型
 * @return 类型名称字符串
 */
const char *lock_object_type_name(lock_object_type_t obj_type);

/* ============================================================
 * 锁初始化（供事务调用）
 * ============================================================ */

/**
 * @brief 初始化事务的锁上下文
 * @param ctx 锁上下文
 * @param mgr 锁管理器
 * @param txn 事务
 */
void lock_ctx_init(lock_ctx_t *ctx, lock_manager_t *mgr, txn_t *txn);

/**
 * @brief 清理事务的锁上下文
 * @param ctx 锁上下文
 */
void lock_ctx_cleanup(lock_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* DB_LOCK_H */
