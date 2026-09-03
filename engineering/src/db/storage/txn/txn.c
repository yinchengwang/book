/**
 * @file txn.c
 * @brief 事务管理器实现
 *
 * 事务管理要点：
 * 1. 每个事务有自己的事务ID
 * 2. 写操作先写 WAL，再写数据
 * 3. 提交时写 COMMIT 日志，刷 WAL，然后刷脏页
 * 4. 回滚时从 WAL 读取 undo 信息，恢复旧值
 * 5. 崩溃恢复时根据 WAL 重做已提交事务，撤销未提交事务
 */

#include "db/wal.h"
#include "db/kv.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* ============================================================
 * 事务常量和类型（本地定义，避免与 db/txn.h 冲突）
 * ============================================================ */

/** 事务返回码 */
#define TXN_OK       0
#define TXN_ERROR   (-1)
#define TXN_NOT_FOUND 1

/** 事务状态 */
typedef enum txn_state_e {
    TXN_STATE_ACTIVE    = 0,
    TXN_STATE_COMMITTED = 1,
    TXN_STATE_ABORTED   = 2,
    TXN_STATE_PREPARED  = 3
} txn_state_t;

/** 无效事务ID */
#define TXN_ID_NONE 0

/** 最大并发事务数 */
#define TXN_MAX_ACTIVE 1024

/* ============================================================
 * MVCC 隔离级别
 * ============================================================ */

/** 事务隔离级别 */
typedef enum txn_isolation_e {
    TXN_ISOLATION_READ_COMMITTED = 0,   /**< 读已提交 */
    TXN_ISOLATION_REPEATABLE_READ = 1,  /**< 可重复读 */
    TXN_ISOLATION_SERIALIZABLE = 2      /**< 可串行化 */
} txn_isolation_t;

/** 元组头（MVCC 版本信息） */
typedef struct tuple_header_s {
    uint32_t t_xmin;       /**< 创建事务 ID */
    uint32_t t_xmax;       /**< 删除事务 ID (0 = 未删除) */
    uint64_t t_csn;        /**< 提交序列号 */
    bool     committed;    /**< 是否已提交 */
} tuple_header_t;

/* ============================================================
 * 内部数据结构
 * ============================================================ */

/** 未提交值记录（用于回滚） */
typedef struct txn_uncommitted_s {
    void         *key;           /**< 键 */
    size_t        key_len;       /**< 键长度 */
    void         *old_value;     /**< 旧值（可能被覆盖或删除） */
    size_t        old_value_len; /**< 旧值长度 */
    bool          deleted;       /**< 是否是删除操作 */
    bool          was_updated;   /**< 是否更新了已存在的键 */
    struct txn_uncommitted_s *next;
} txn_uncommitted_t;

/** 事务上下文 */
struct txn_s {
    uint32_t         txn_id;         /**< 事务ID */
    txn_state_t      state;          /**< 事务状态 */
    kv_t            *db;             /**< KV 数据库 */
    wal_t           *wal;            /**< WAL 日志 */
    uint64_t         start_lsn;      /**< 事务开始时的 LSN */
    uint64_t         start_time;     /**< 开始时间（毫秒） */
    uint32_t         last_lsn;       /**< 最后一条日志的 LSN */

    /* MVCC 字段 */
    txn_isolation_t  isolation;      /**< 隔离级别 */
    uint64_t         snapshot_lsn;   /**< 快照 LSN（事务开始时） */
    bool             is_mvcc;        /**< 是否使用 MVCC */

    /* 未提交值列表（用于回滚） */
    txn_uncommitted_t *uncommitted;

    /* 错误信息 */
    char            *error_msg;
};

/* ============================================================
 * 全局事务追踪
 * ============================================================ */

/** 活动事务表 */
static txn_t *g_active_txns[TXN_MAX_ACTIVE];
static size_t g_txn_count = 0;
static uint32_t g_next_txn_id = 1;

/** 获取下一个事务ID */
static uint32_t txn_next_id(void) {
    return g_next_txn_id++;
}

/* ============================================================
 * 工具函数
 * ============================================================ */

/** 设置错误信息 */
void txn_set_error(txn_t *txn, const char *msg) {
    if (txn && txn->error_msg) {
        free(txn->error_msg);
    }
    if (txn && msg) {
        txn->error_msg = strdup(msg);
    }
}

/** 获取当前时间（毫秒） */
static uint64_t current_time_ms(void) {
    return (uint64_t)(time(NULL) * 1000);
}

/* ============================================================
 * 事务生命周期
 * ============================================================ */

txn_t *txn_begin(kv_t *db) {
    if (!db) return NULL;

    /* 分配事务 */
    txn_t *txn = (txn_t *)calloc(1, sizeof(txn_t));
    if (!txn) return NULL;

    txn->txn_id = txn_next_id();
    txn->state = TXN_STATE_ACTIVE;
    txn->db = db;
    txn->wal = db->wal;  /* 从 KV 获取 WAL */
    txn->start_time = current_time_ms();
    txn->start_lsn = 0;
    txn->last_lsn = 0;

    /* 写 BEGIN 日志 */
    if (txn->wal) {
        txn->start_lsn = wal_write_begin(txn->wal, txn->txn_id);
    }

    /* 注册到活动事务表 */
    if (g_txn_count < TXN_MAX_ACTIVE) {
        g_active_txns[g_txn_count++] = txn;
    }

    return txn;
}

int txn_commit(txn_t *txn) {
    if (!txn) return TXN_ERROR;
    if (txn->state != TXN_STATE_ACTIVE) {
        txn_set_error(txn, "Transaction not active");
        return TXN_ERROR;
    }

    /* 写 COMMIT 日志 */
    if (txn->wal) {
        txn->last_lsn = wal_write_commit(txn->wal, txn->txn_id);
        /* 刷 WAL 保证日志持久化 */
        wal_flush(txn->wal);
    }

    /* 刷脏页（由 KV 层处理） */
    /* 注意：这里假设 KV 层会管理脏页 */

    txn->state = TXN_STATE_COMMITTED;

    /* 从活动事务表移除 */
    for (size_t i = 0; i < g_txn_count; i++) {
        if (g_active_txns[i] == txn) {
            g_active_txns[i] = g_active_txns[--g_txn_count];
            break;
        }
    }

    return TXN_OK;
}

int txn_rollback(txn_t *txn) {
    if (!txn) return TXN_ERROR;
    if (txn->state != TXN_STATE_ACTIVE) {
        txn_set_error(txn, "Transaction not active");
        return TXN_ERROR;
    }

    /* 逆向应用未提交的修改 */
    txn_uncommitted_t *uc = txn->uncommitted;
    while (uc) {
        if (uc->was_updated || uc->deleted) {
            /* 恢复旧值 */
            if (uc->old_value && uc->old_value_len > 0) {
                kv_put(txn->db, uc->key, uc->key_len,
                       uc->old_value, uc->old_value_len);
            }
        } else if (!uc->deleted && uc->old_value == NULL) {
            /* 是新增的键，需要删除 */
            kv_delete(txn->db, uc->key, uc->key_len);
        }
        uc = uc->next;
    }

    /* 写 ABORT 日志 */
    if (txn->wal) {
        wal_write_abort(txn->wal, txn->txn_id);
    }

    txn->state = TXN_STATE_ABORTED;

    /* 从活动事务表移除 */
    for (size_t i = 0; i < g_txn_count; i++) {
        if (g_active_txns[i] == txn) {
            g_active_txns[i] = g_active_txns[--g_txn_count];
            break;
        }
    }

    return TXN_OK;
}

void txn_free(txn_t *txn) {
    if (!txn) return;

    /* 如果是活动事务，先从活动事务表移除 */
    if (txn->state == TXN_STATE_ACTIVE) {
        for (size_t i = 0; i < g_txn_count; i++) {
            if (g_active_txns[i] == txn) {
                g_active_txns[i] = g_active_txns[--g_txn_count];
                break;
            }
        }
    }

    /* 清理未提交值列表 */
    txn_uncommitted_t *uc = txn->uncommitted;
    while (uc) {
        txn_uncommitted_t *next = uc->next;
        free(uc->key);
        free(uc->old_value);
        free(uc);
        uc = next;
    }

    if (txn->error_msg) free(txn->error_msg);
    free(txn);
}

/* ============================================================
 * 事务操作
 * ============================================================ */

/**
 * 添加未提交记录到链表
 */
static void txn_add_uncommitted(txn_t *txn,
                                const void *key, size_t key_len,
                                const void *old_value, size_t old_value_len,
                                bool deleted, bool was_updated) {
    txn_uncommitted_t *uc = (txn_uncommitted_t *)malloc(sizeof(txn_uncommitted_t));
    if (!uc) return;

    uc->key = malloc(key_len);
    memcpy(uc->key, key, key_len);
    uc->key_len = key_len;

    if (old_value && old_value_len > 0) {
        uc->old_value = malloc(old_value_len);
        memcpy(uc->old_value, old_value, old_value_len);
        uc->old_value_len = old_value_len;
    } else {
        uc->old_value = NULL;
        uc->old_value_len = 0;
    }

    uc->deleted = deleted;
    uc->was_updated = was_updated;
    uc->next = txn->uncommitted;
    txn->uncommitted = uc;
}

int txn_put(txn_t *txn,
            const void *key, size_t key_len,
            const void *value, size_t value_len) {
    if (!txn || txn->state != TXN_STATE_ACTIVE) {
        return TXN_ERROR;
    }

    /* 检查键是否存在，获取旧值 */
    void *old_value = NULL;
    size_t old_value_len = 0;
    bool existed = (kv_get(txn->db, key, key_len, &old_value, &old_value_len) == KV_OK);

    /* 写 WAL 日志 */
    if (txn->wal) {
        if (existed) {
            /* 更新操作：记录旧值用于 undo */
            txn->last_lsn = wal_write_update(txn->wal, txn->txn_id,
                                              key, key_len,
                                              old_value, old_value_len,
                                              value, value_len);
        } else {
            /* 插入操作 */
            txn->last_lsn = wal_write_insert(txn->wal, txn->txn_id,
                                              key, key_len,
                                              value, value_len);
        }
    }

    /* 记录到未提交列表（用于回滚） */
    txn_add_uncommitted(txn, key, key_len, old_value, old_value_len, false, existed);

    /* 执行实际更新 */
    kv_result_t result = kv_put(txn->db, key, key_len, value, value_len);

    if (old_value) free(old_value);

    return (result == KV_OK) ? TXN_OK : TXN_ERROR;
}

int txn_get(txn_t *txn,
            const void *key, size_t key_len,
            void **out_value, size_t *out_len) {
    if (!txn || txn->state != TXN_STATE_ACTIVE) {
        return TXN_ERROR;
    }

    /* 先检查未提交列表（用于检测删除） */
    for (txn_uncommitted_t *uc = txn->uncommitted; uc; uc = uc->next) {
        if (uc->key_len == key_len && memcmp(uc->key, key, key_len) == 0) {
            /* 在未提交列表中找到该键 */
            if (uc->deleted) {
                /* 已标记为删除，返回不存在 */
                return TXN_NOT_FOUND;
            }
            /* 否则从数据库读取（因为 kv_put 已经更新了数据库） */
            break;
        }
    }

    /* 从数据库读取（包含已提交和已应用的未提交修改） */
    kv_result_t result = kv_get(txn->db, key, key_len, out_value, out_len);
    return (result == KV_OK) ? TXN_OK : TXN_NOT_FOUND;
}

int txn_delete(txn_t *txn, const void *key, size_t key_len) {
    if (!txn || txn->state != TXN_STATE_ACTIVE) {
        return TXN_ERROR;
    }

    /* 检查键是否存在，获取旧值 */
    void *old_value = NULL;
    size_t old_value_len = 0;
    bool existed = (kv_get(txn->db, key, key_len, &old_value, &old_value_len) == KV_OK);

    if (!existed) {
        return TXN_NOT_FOUND;
    }

    /* 写 WAL 日志 */
    if (txn->wal) {
        txn->last_lsn = wal_write_delete(txn->wal, txn->txn_id,
                                          key, key_len,
                                          old_value, old_value_len);
    }

    /* 记录到未提交列表 */
    txn_add_uncommitted(txn, key, key_len, old_value, old_value_len, true, true);

    /* 执行实际删除 */
    kv_result_t result = kv_delete(txn->db, key, key_len);

    if (old_value) free(old_value);

    return (result == KV_OK) ? TXN_OK : TXN_ERROR;
}

bool txn_exists(txn_t *txn, const void *key, size_t key_len) {
    return txn_get(txn, key, key_len, NULL, NULL) == TXN_OK;
}

/* ============================================================
 * 事务查询
 * ============================================================ */

uint32_t txn_id(const txn_t *txn) {
    return txn ? txn->txn_id : TXN_ID_NONE;
}

txn_state_t txn_state(const txn_t *txn) {
    return txn ? txn->state : TXN_STATE_ABORTED;
}

const char *txn_errmsg(const txn_t *txn) {
    if (!txn) return "Invalid transaction";
    return txn->error_msg ? txn->error_msg : "OK";
}

uint64_t txn_start_time(const txn_t *txn) {
    return txn ? txn->start_time : 0;
}

/* ============================================================
 * 工具函数
 * ============================================================ */

size_t txn_active_count(void) {
    return g_txn_count;
}

void txn_print_active(void) {
    printf("Active transactions: %zu\n", g_txn_count);
    for (size_t i = 0; i < g_txn_count; i++) {
        txn_t *txn = g_active_txns[i];
        if (txn) {
            const char *state_str = "UNKNOWN";
            switch (txn->state) {
                case TXN_STATE_ACTIVE:    state_str = "ACTIVE"; break;
                case TXN_STATE_COMMITTED: state_str = "COMMITTED"; break;
                case TXN_STATE_ABORTED:   state_str = "ABORTED"; break;
                case TXN_STATE_PREPARED:  state_str = "PREPARED"; break;
            }
            printf("  txn_id=%u state=%s start_time=%llu\n",
                   txn->txn_id, state_str, (unsigned long long)txn->start_time);
        }
    }
}

/* ============================================================
 * MVCC 快照隔离
 * ============================================================ */

/**
 * @brief 获取事务的快照 LSN
 * @param txn 事务句柄
 * @return 快照 LSN，事务不存在返回 0
 */
uint64_t txn_get_snapshot_lsn(const txn_t *txn) {
    if (!txn) return 0;
    return txn->snapshot_lsn;
}

/**
 * @brief 检查事务是否使用 MVCC
 * @param txn 事务句柄
 * @return true 如果使用 MVCC
 */
bool txn_is_mvcc(const txn_t *txn) {
    if (!txn) return false;
    return txn->is_mvcc;
}

/**
 * @brief 获取事务隔离级别
 * @param txn 事务句柄
 * @return 隔离级别
 */
txn_isolation_t txn_get_isolation_level(const txn_t *txn) {
    if (!txn) return TXN_ISOLATION_READ_COMMITTED;
    return txn->isolation;
}

/**
 * @brief 开始 MVCC 事务
 *
 * 创建一个使用 MVCC 快照隔离的事务，获取 IS 锁。
 * 在修改数据时会升级为 X 锁。
 *
 * @param txn 事务句柄
 * @param level 隔离级别
 * @return TXN_OK 成功，TXN_ERROR 失败
 */
int txn_begin_mvcc(txn_t *txn, txn_isolation_t level) {
    if (!txn) return TXN_ERROR;
    if (txn->state != TXN_STATE_ACTIVE) {
        txn_set_error(txn, "Transaction not active");
        return TXN_ERROR;
    }

    /* 设置 MVCC 隔离级别 */
    txn->isolation = level;
    txn->is_mvcc = true;

    /* 记录快照 LSN */
    if (txn->wal) {
        txn->snapshot_lsn = wal_get_lsn(txn->wal);
    } else {
        txn->snapshot_lsn = txn->start_lsn;
    }

    /* 获取 IS 锁（意向共享锁） */
    lock_acquire(txn->db->lock_mgr, txn, LOCK_DATABASE, 0, 0, LOCK_MODE_IS, 1000);

    return TXN_OK;
}

/**
 * @brief 检查元组对当前事务是否可见
 *
 * MVCC 可见性判断规则：
 * 1. 事务自己的修改始终可见
 * 2. xmin 事务已提交且在快照之前 -> 可见
 * 3. xmax 事务已提交且在快照之前 -> 不可见（已被删除）
 *
 * @param txn 事务句柄
 * @param tuple 元组头（包含 xmin/xmax）
 * @return 1 可见，0 不可见
 */
int txn_visibility_check(txn_t *txn, const tuple_header_t *tuple) {
    if (!txn || !tuple) return 0;

    /* 事务自己的修改始终可见 */
    if (tuple->t_xmin == txn->txn_id) return 1;

    /* 检查 xmin 是否在快照之前 */
    if (tuple->t_xmin < txn->snapshot_lsn && tuple->committed) {
        /* xmin 已提交且在快照之前，检查 xmax */
        if (tuple->t_xmax == 0) {
            /* 未被删除，可见 */
            return 1;
        }
        if (tuple->t_xmax >= txn->snapshot_lsn) {
            /* 删除操作在快照之后，仍可见 */
            return 1;
        }
        /* 已被删除且删除操作在快照之前，不可见 */
        return 0;
    }

    /* xmin 在快照之后或未提交，不可见 */
    return 0;
}

/**
 * @brief 写修改操作的 WAL（带 MVCC 信息）
 *
 * 在 MVCC 模式下，写操作会记录 xmin/xmax 信息到 WAL。
 *
 * @param txn 事务句柄
 * @param key 键
 * @param key_len 键长度
 * @param old_value 旧值（更新/删除时）
 * @param old_len 旧值长度
 * @param new_value 新值（插入/更新时）
 * @param new_len 新值长度
 * @param is_delete 是否是删除操作
 * @return WAL LSN
 */
uint64_t txn_write_mvcc_wal(txn_t *txn,
                            const void *key, size_t key_len,
                            const void *old_value, size_t old_len,
                            const void *new_value, size_t new_len,
                            bool is_delete) {
    if (!txn || !txn->wal) return 0;

    uint64_t lsn = 0;

    if (is_delete) {
        lsn = wal_write_delete(txn->wal, txn->txn_id,
                               key, key_len,
                               old_value, old_len);
    } else if (old_value && old_len > 0) {
        lsn = wal_write_update(txn->wal, txn->txn_id,
                               key, key_len,
                               old_value, old_len,
                               new_value, new_len);
    } else {
        lsn = wal_write_insert(txn->wal, txn->txn_id,
                               key, key_len,
                               new_value, new_len);
    }

    txn->last_lsn = lsn;
    return lsn;
}

/**
 * @brief 升级锁到排他模式
 *
 * 在 MVCC 模式下，修改数据前需要升级锁。
 *
 * @param txn 事务句柄
 * @return TXN_OK 成功，TXN_ERROR 失败
 */
int txn_upgrade_lock_to_exclusive(txn_t *txn) {
    if (!txn) return TXN_ERROR;

    /* 释放 IS 锁 */
    lock_release(txn->db->lock_mgr, txn, LOCK_DATABASE, 0, LOCK_MODE_IS);

    /* 获取 X 锁 */
    if (lock_acquire(txn->db->lock_mgr, txn, LOCK_DATABASE, 0, 0, LOCK_MODE_X, 1000) != LOCK_OK) {
        txn_set_error(txn, "Failed to acquire exclusive lock");
        return TXN_ERROR;
    }

    return TXN_OK;
}