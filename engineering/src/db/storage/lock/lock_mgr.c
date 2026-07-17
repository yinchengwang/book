/**
 * @file lock_mgr.c
 * @brief 数据库锁管理器实现
 *
 * 支持多粒度锁：表锁、页锁、行锁
 * 支持意向锁：IS/IX/SIX
 * 支持死锁检测和乐观锁
 *
 * 锁层级关系：
 *   DATABASE ──▶ TABLE ──▶ PAGE ──▶ ROW
 *                 IS/IX    IS/IX   S/X
 *
 * 锁兼容矩阵：
 *         |  S   X   IS  IX  SIX
 *   ------+-----------------------
 *   S     |  ✓   ✗   ✓   ✗   ✗
 *   X     |  ✗   ✗   ✗   ✗   ✗
 *   IS    |  ✓   ✗   ✓   ✓   ✓
 *   IX    |  ✗   ✗   ✓   ✓   ✗
 *   SIX   |  ✗   ✗   ✓   ✗   ✗
 */

#include <db/lock.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Windows 平台定义 */
#if defined(_WIN32) || defined(_WIN64)
    #include <windows.h>
#endif

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

/**
 * @brief 获取当前时间（毫秒）
 */
static uint64_t get_current_time_ms(void)
{
#if defined(_WIN32) || defined(_WIN64)
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t now = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    return now / 10000;  /* 100 纳秒转毫秒 */
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
#endif
}

/**
 * @brief 计算锁对象的哈希值
 */
static uint32_t lock_hash(uint64_t object_id)
{
    /* 简单的哈希函数 */
    return (uint32_t)((object_id * 0x9e3779b9) >> 16) & (LOCK_HASH_SIZE - 1);
}

/**
 * @brief 检查锁模式是否兼容
 *
 * 兼容矩阵：
 * - S (Shared) 与 S、IS 兼容
 * - X (Exclusive) 与任何锁都不兼容
 * - IS (Intention Shared) 与 S、IS、IX、SIX 兼容
 * - IX (Intention Exclusive) 与 IS、IX 兼容
 * - SIX (Shared Intention Exclusive) 与 IS 兼容
 */
static bool check_lock_compatibility(lock_mode_t granted, lock_mode_t requested)
{
    /* 兼容矩阵定义 */
    static const bool compat_matrix[5][5] = {
        /*         S    X   IS   IX  SIX */
        /* S  */ { true, false, true, false, false },
        /* X  */ { false, false, false, false, false },
        /* IS */ { true, false, true, true, true },
        /* IX */ { false, false, true, true, false },
        /* SIX*/ { false, false, true, false, false }
    };

    return compat_matrix[granted][requested];
}

/**
 * @brief 获取锁模式的强度等级（用于确定已授予的最高级别锁）
 *
 * 锁强度排序：S < IS < IX < SIX < X
 */
static int lock_mode_strength(lock_mode_t mode)
{
    switch (mode) {
        case LOCK_MODE_S:   return 1;
        case LOCK_MODE_IS:  return 2;
        case LOCK_MODE_IX:  return 3;
        case LOCK_MODE_SIX: return 4;
        case LOCK_MODE_X:   return 5;
        default:            return 0;
    }
}

/**
 * @brief 判断锁模式是否为共享锁
 */
static bool is_shared_lock(lock_mode_t mode)
{
    return mode == LOCK_MODE_S || mode == LOCK_MODE_IS || mode == LOCK_MODE_SIX;
}

/**
 * @brief 判断锁模式是否为排他锁
 */
static bool is_exclusive_lock(lock_mode_t mode)
{
    return mode == LOCK_MODE_X || mode == LOCK_MODE_IX;
}

/**
 * @brief 创建新的锁请求
 */
static lock_request_t *lock_request_create(txn_t *txn, lock_mode_t mode)
{
    lock_request_t *req = (lock_request_t *)malloc(sizeof(lock_request_t));
    if (!req) {
        return NULL;
    }

    req->txn = txn;
    req->mode = mode;
    req->granted = false;
    req->wait_start_time = get_current_time_ms();
    req->recursion = 0;
    req->next = NULL;
    req->prev = NULL;

    return req;
}

/**
 * @brief 释放锁请求
 */
static void lock_request_destroy(lock_request_t *req)
{
    if (req) {
        free(req);
    }
}

/**
 * @brief 在等待队列中查找事务的锁请求
 */
static lock_request_t *find_request_in_queue(lock_request_t *queue, txn_t *txn)
{
    lock_request_t *cur = queue;
    while (cur) {
        if (cur->txn == txn) {
            return cur;
        }
        cur = cur->next;
    }
    return NULL;
}

/**
 * @brief 释放锁条目及其所有资源
 */
static void lock_entry_destroy(lock_entry_t *entry)
{
    if (!entry) {
        return;
    }

    /* 释放等待队列中的所有请求 */
    lock_request_t *req = entry->wait_queue;
    while (req) {
        lock_request_t *next = req->next;
        lock_request_destroy(req);
        req = next;
    }

    /* 释放共享锁持有者链表 */
    req = entry->shared_holders;
    while (req) {
        lock_request_t *next = req->next;
        lock_request_destroy(req);
        req = next;
    }

    free(entry);
}

/**
 * @brief 唤醒等待队列中的下一个请求
 */
static void wake_up_next_waiter(lock_entry_t *entry)
{
    if (!entry || !entry->wait_queue) {
        return;
    }

    /* 找到第一个未授予的请求 */
    lock_request_t *waiter = entry->wait_queue;
    while (waiter && waiter->granted) {
        waiter = waiter->next;
    }

    if (waiter) {
        waiter->granted = true;
        /* 标记事务可以继续执行（实际唤醒由调用者处理） */
    }
}

/* ============================================================
 * 锁管理器公共 API
 * ============================================================ */

lock_manager_t *lock_mgr_create(void)
{
    lock_manager_t *mgr = (lock_manager_t *)calloc(1, sizeof(lock_manager_t));
    if (!mgr) {
        return NULL;
    }

    /* 初始化锁哈希表 */
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        mgr->lock_table[i] = NULL;
    }

    /* 默认配置 */
    mgr->deadlock_detection_enabled = true;
    mgr->deadlock_timeout_ms = LOCK_DEFAULT_TIMEOUT_MS;
    mgr->escalation_policy = LOCK_ESCALATION_AUTO;

    /* 初始化统计信息 */
    atomic_init(&mgr->lock_requests, 0);
    atomic_init(&mgr->lock_waits, 0);
    atomic_init(&mgr->deadlocks, 0);
    atomic_init(&mgr->lock_escalations, 0);

    return mgr;
}

void lock_mgr_destroy(lock_manager_t *mgr)
{
    if (!mgr) {
        return;
    }

    /* 检查是否有未释放的锁 */
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        lock_entry_t *entry = mgr->lock_table[i];
        while (entry) {
            lock_entry_t *next = entry->hash_next;
            /* 如果有活动锁，发出警告但不强制释放 */
            if (entry->exclusive_holder || entry->shared_holders) {
                /* 记录泄漏的锁信息（调试用） */
                fprintf(stderr, "Warning: destroyed lock manager with active locks\n");
            }
            lock_entry_destroy(entry);
            entry = next;
        }
    }

    free(mgr);
}

lock_result_t lock_acquire(lock_manager_t *mgr, txn_t *txn,
                           lock_object_type_t obj_type, uint64_t object_id,
                           uint64_t table_id, lock_mode_t mode,
                           uint32_t timeout_ms)
{
    (void)obj_type;  /* 未使用参数 */

    if (!mgr || !txn) {
        return LOCK_ERROR;
    }

    atomic_fetch_add(&mgr->lock_requests, 1);

    uint32_t hash = lock_hash(object_id);

    /* 在哈希表中查找或创建锁条目 */
    lock_entry_t *entry = NULL;
    lock_entry_t **entry_ptr = &mgr->lock_table[hash];

    while (*entry_ptr) {
        if ((*entry_ptr)->object_id == object_id) {
            entry = *entry_ptr;
            break;
        }
        entry_ptr = &((*entry_ptr)->hash_next);
    }

    /* 如果锁条目不存在，创建新的 */
    if (!entry) {
        entry = (lock_entry_t *)calloc(1, sizeof(lock_entry_t));
        if (!entry) {
            return LOCK_ERROR;
        }

        entry->obj_type = obj_type;
        entry->object_id = object_id;
        entry->table_id = table_id;
        entry->granted_mode = LOCK_MODE_S;  /* 最低级别 */
        entry->exclusive_holder = NULL;
        entry->shared_holders = NULL;
        entry->wait_queue = NULL;
        atomic_init(&entry->ref_count, 1);
        atomic_init(&entry->row_lock_count, 0);
        atomic_init(&entry->recursion_count, 0);
        entry->hash_next = NULL;
        entry->hash_prev_ptr = entry_ptr;
        *entry_ptr = entry;
    } else {
        atomic_fetch_add(&entry->ref_count, 1);
    }

    /* 检查是否已经持有该锁（可重入锁） */
    if (entry->exclusive_holder == txn ||
        find_request_in_queue(entry->shared_holders, txn)) {
        /* 可重入：直接成功 */
        atomic_fetch_sub(&entry->ref_count, 1);
        return LOCK_OK;
    }

    /* 检查是否可以直接授予 */
    bool can_grant = false;

    /* 如果没有持有者，检查兼容性 */
    if (!entry->exclusive_holder &&
        (!entry->shared_holders || is_shared_lock(mode))) {
        can_grant = check_lock_compatibility(entry->granted_mode, mode);
    }

    if (can_grant) {
        /* 直接授予锁 */
        if (is_shared_lock(mode)) {
            /* 共享锁：添加到共享持有者链表 */
            lock_request_t *req = lock_request_create(txn, mode);
            if (!req) {
                atomic_fetch_sub(&entry->ref_count, 1);
                return LOCK_ERROR;
            }
            req->granted = true;
            req->next = entry->shared_holders;
            if (entry->shared_holders) {
                entry->shared_holders->prev = req;
            }
            entry->shared_holders = req;
        } else {
            /* 排他锁：设置独占持有者 */
            entry->exclusive_holder = txn;
            atomic_init(&entry->recursion_count, 1);
        }

        /* 更新已授予的最高级别锁 */
        if (lock_mode_strength(mode) > lock_mode_strength(entry->granted_mode)) {
            entry->granted_mode = mode;
        }

        atomic_fetch_sub(&entry->ref_count, 1);
        return LOCK_OK;
    }

    /* 需要等待：添加到等待队列 */
    atomic_fetch_add(&mgr->lock_waits, 1);

    lock_request_t *req = lock_request_create(txn, mode);
    if (!req) {
        atomic_fetch_sub(&entry->ref_count, 1);
        return LOCK_ERROR;
    }

    /* 添加到等待队列尾部（FIFO） */
    if (!entry->wait_queue) {
        entry->wait_queue = req;
    } else {
        lock_request_t *tail = entry->wait_queue;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = req;
        req->prev = tail;
    }

    /* 等待锁授予或超时 */
    uint64_t start_time = get_current_time_ms();
    uint32_t wait_timeout = timeout_ms > 0 ? timeout_ms : LOCK_DEFAULT_TIMEOUT_MS;

    while (!req->granted) {
        /* 检查超时 */
        uint64_t elapsed = get_current_time_ms() - start_time;
        if (elapsed >= wait_timeout) {
            /* 从等待队列中移除 */
            if (req->prev) {
                req->prev->next = req->next;
            } else {
                entry->wait_queue = req->next;
            }
            if (req->next) {
                req->next->prev = req->prev;
            }
            lock_request_destroy(req);
            atomic_fetch_sub(&entry->ref_count, 1);
            return LOCK_TIMEOUT;
        }

        /* 检查死锁（如果启用） */
        if (mgr->deadlock_detection_enabled) {
            if (lock_detect_deadlock(mgr)) {
                /* 检测到死锁，选择一个 victim 回滚 */
                txn_t *victim = lock_detect_and_resolve_deadlock(mgr);
                if (victim == txn) {
                    /* 当前事务是 victim */
                    if (req->prev) {
                        req->prev->next = req->next;
                    } else {
                        entry->wait_queue = req->next;
                    }
                    if (req->next) {
                        req->next->prev = req->prev;
                    }
                    lock_request_destroy(req);
                    atomic_fetch_sub(&entry->ref_count, 1);
                    atomic_fetch_add(&mgr->deadlocks, 1);
                    return LOCK_DEADLOCK;
                }
            }
        }

        /* 简单的自旋等待（实际应该使用条件变量） */
#if defined(_WIN32)
        Sleep(1);  /* Windows: 1毫秒 */
#else
        usleep(1000);  /* Unix: 1000微秒 = 1毫秒 */
#endif
    }

    /* 锁已授予 */
    atomic_fetch_sub(&entry->ref_count, 1);
    return LOCK_OK;
}

void lock_release(lock_manager_t *mgr, txn_t *txn,
                 lock_object_type_t obj_type, uint64_t object_id,
                 lock_mode_t mode)
{
    (void)obj_type;  /* 未使用参数 */

    if (!mgr || !txn) {
        return;
    }

    uint32_t hash = lock_hash(object_id);

    /* 查找锁条目 */
    lock_entry_t *entry = mgr->lock_table[hash];
    while (entry) {
        if (entry->object_id == object_id) {
            break;
        }
        entry = entry->hash_next;
    }

    if (!entry) {
        /* 没有找到锁条目，可能是已经释放或者从未获取 */
        return;
    }

    /* 从持有者中移除 */
    if (is_exclusive_lock(mode) && entry->exclusive_holder == txn) {
        entry->exclusive_holder = NULL;
        atomic_store(&entry->recursion_count, 0);
    } else if (is_shared_lock(mode)) {
        /* 从共享持有者链表中移除 */
        lock_request_t *req = entry->shared_holders;
        while (req) {
            if (req->txn == txn) {
                /* 从链表中移除 */
                if (req->prev) {
                    req->prev->next = req->next;
                } else {
                    entry->shared_holders = req->next;
                }
                if (req->next) {
                    req->next->prev = req->prev;
                }
                lock_request_destroy(req);
                break;
            }
            req = req->next;
        }
    }

    /* 重新计算已授予的最高级别锁 */
    lock_mode_t new_granted = LOCK_MODE_S;
    if (entry->exclusive_holder) {
        new_granted = LOCK_MODE_X;
    } else if (entry->shared_holders) {
        /* 检查是否有 IX 或 SIX */
        lock_request_t *req = entry->shared_holders;
        while (req) {
            if (req->mode == LOCK_MODE_SIX) {
                new_granted = LOCK_MODE_SIX;
                break;
            } else if (req->mode == LOCK_MODE_IX) {
                new_granted = LOCK_MODE_IX;
            } else if (req->mode == LOCK_MODE_IS && new_granted < LOCK_MODE_IS) {
                new_granted = LOCK_MODE_IS;
            } else if (req->mode == LOCK_MODE_S && new_granted < LOCK_MODE_S) {
                new_granted = LOCK_MODE_S;
            }
            req = req->next;
        }
    }
    entry->granted_mode = new_granted;

    /* 唤醒等待队列中的下一个请求 */
    wake_up_next_waiter(entry);
}

void lock_release_all(lock_manager_t *mgr, txn_t *txn)
{
    if (!mgr || !txn) {
        return;
    }

    /* 遍历所有锁条目，释放该事务持有的所有锁 */
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        lock_entry_t *entry = mgr->lock_table[i];
        while (entry) {
            /* 检查是否独占持有 */
            if (entry->exclusive_holder == txn) {
                lock_release(mgr, txn, entry->obj_type, entry->object_id, LOCK_MODE_X);
            }

            /* 检查共享持有者链表 */
            lock_request_t *req = entry->shared_holders;
            while (req) {
                if (req->txn == txn) {
                    lock_release(mgr, txn, entry->obj_type, entry->object_id, req->mode);
                    break;  /* 只能持有一种模式的锁 */
                }
                req = req->next;
            }

            entry = entry->hash_next;
        }
    }
}

bool lock_held(lock_manager_t *mgr, txn_t *txn,
               lock_object_type_t obj_type, uint64_t object_id)
{
    (void)obj_type;  /* 未使用参数 */

    if (!mgr || !txn) {
        return false;
    }

    uint32_t hash = lock_hash(object_id);

    lock_entry_t *entry = mgr->lock_table[hash];
    while (entry) {
        if (entry->object_id == object_id) {
            /* 检查独占持有 */
            if (entry->exclusive_holder == txn) {
                return true;
            }
            /* 检查共享持有 */
            if (find_request_in_queue(entry->shared_holders, txn)) {
                return true;
            }
            return false;
        }
        entry = entry->hash_next;
    }

    return false;
}

bool lock_already_held(lock_manager_t *mgr, txn_t *txn,
                      lock_object_type_t obj_type, uint64_t object_id)
{
    return lock_held(mgr, txn, obj_type, object_id);
}

/* ============================================================
 * 锁升级
 * ============================================================ */

bool lock_escalate_row_to_page(lock_manager_t *mgr, txn_t *txn,
                               uint64_t table_id, uint64_t page_id)
{
    /* 锁升级策略：
     * 当同一页上的行锁数量超过阈值时，将行锁升级为页锁
     * 实现：从所有行锁中获取一个，然后请求页锁
     */
    (void)mgr;
    (void)txn;
    (void)table_id;
    (void)page_id;

    /* TODO: 实现锁升级逻辑 */
    atomic_fetch_add(&mgr->lock_escalations, 1);
    return false;  /* 暂时不支持 */
}

bool lock_escalate_page_to_table(lock_manager_t *mgr, txn_t *txn,
                                 uint64_t table_id)
{
    /* 锁升级策略：
     * 当同一表上的页锁数量超过阈值时，将页锁升级为表锁
     */
    (void)mgr;
    (void)txn;
    (void)table_id;

    atomic_fetch_add(&mgr->lock_escalations, 1);
    return false;  /* 暂时不支持 */
}

lock_result_t lock_deescalate(lock_manager_t *mgr, txn_t *txn,
                               lock_object_type_t obj_type, uint64_t object_id,
                               lock_mode_t mode)
{
    (void)mgr;
    (void)txn;
    (void)obj_type;
    (void)object_id;
    (void)mode;

    /* TODO: 实现锁降级逻辑 */
    return LOCK_ERROR;
}

/* ============================================================
 * 死锁检测
 * ============================================================ */

/**
 * @brief 深度优先搜索检测死锁
 *
 * 使用等待图 (Wait-For Graph) 检测死锁：
 * - 节点：事务
 * - 边：事务 T1 等待事务 T2 持有的锁
 *
 * 如果图中存在环，则存在死锁。
 */
static bool dfs_detect_deadlock(lock_manager_t *mgr, txn_t *txn,
                                txn_t **visited, int *visited_count, int max_count)
{
    if (!txn) {
        return false;
    }

    /* 检查是否已访问（检测环） */
    for (int i = 0; i < *visited_count; i++) {
        if (visited[i] == txn) {
            return true;  /* 发现环 */
        }
    }

    /* 添加到已访问列表 */
    if (*visited_count >= max_count) {
        return false;
    }
    visited[(*visited_count)++] = txn;

    /* 遍历该事务等待的所有锁 */
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        lock_entry_t *entry = mgr->lock_table[i];
        while (entry) {
            /* 检查等待队列中的请求 */
            lock_request_t *req = entry->wait_queue;
            while (req) {
                if (req->txn == txn && !req->granted) {
                    /* 当前事务在等待这个锁，检查持有者 */
                    txn_t *holder = NULL;

                    if (entry->exclusive_holder) {
                        holder = entry->exclusive_holder;
                    } else {
                        /* 对于共享锁，随机选择一个持有者 */
                        holder = entry->shared_holders ? entry->shared_holders->txn : NULL;
                    }

                    if (holder && holder != txn) {
                        /* 递归检查持有者是否在等待其他锁 */
                        if (dfs_detect_deadlock(mgr, holder, visited, visited_count, max_count)) {
                            return true;
                        }
                    }
                }
                req = req->next;
            }
            entry = entry->hash_next;
        }
    }

    return false;
}

bool lock_detect_deadlock(lock_manager_t *mgr)
{
    if (!mgr || !mgr->deadlock_detection_enabled) {
        return false;
    }

    txn_t *visited[256];
    int visited_count = 0;

    /* 对每个等待中的事务进行 DFS */
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        lock_entry_t *entry = mgr->lock_table[i];
        while (entry) {
            lock_request_t *req = entry->wait_queue;
            while (req) {
                if (!req->granted) {
                    visited_count = 0;
                    if (dfs_detect_deadlock(mgr, req->txn, visited, &visited_count, 256)) {
                        return true;  /* 发现死锁 */
                    }
                }
                req = req->next;
            }
            entry = entry->hash_next;
        }
    }

    return false;
}

txn_t *lock_detect_and_resolve_deadlock(lock_manager_t *mgr)
{
    if (!lock_detect_deadlock(mgr)) {
        return NULL;
    }

    /* 找到等待图中的 victim 事务
     * 简单策略：选择等待时间最长的那个
     */
    txn_t *victim = NULL;
    uint64_t max_wait = 0;

    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        lock_entry_t *entry = mgr->lock_table[i];
        while (entry) {
            lock_request_t *req = entry->wait_queue;
            while (req) {
                if (!req->granted) {
                    uint64_t wait_time = get_current_time_ms() - req->wait_start_time;
                    if (wait_time > max_wait) {
                        max_wait = wait_time;
                        victim = req->txn;
                    }
                }
                req = req->next;
            }
            entry = entry->hash_next;
        }
    }

    return victim;  /* 返回 victim，由调用者负责回滚 */
}

int lock_get_deadlock_cycle(lock_manager_t *mgr, txn_t **out_txns, int max_count)
{
    if (!mgr || !out_txns || max_count <= 0) {
        return 0;
    }

    int count = 0;

    /* 简单实现：收集所有等待中的事务 */
    for (int i = 0; i < LOCK_HASH_SIZE && count < max_count; i++) {
        lock_entry_t *entry = mgr->lock_table[i];
        while (entry && count < max_count) {
            lock_request_t *req = entry->wait_queue;
            while (req && count < max_count) {
                if (!req->granted) {
                    /* 检查是否已添加 */
                    bool found = false;
                    for (int j = 0; j < count; j++) {
                        if (out_txns[j] == req->txn) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        out_txns[count++] = req->txn;
                    }
                }
                req = req->next;
            }
            entry = entry->hash_next;
        }
    }

    return count;
}

uint64_t lock_get_deadlock_count(lock_manager_t *mgr)
{
    if (!mgr) {
        return 0;
    }
    return atomic_load(&mgr->deadlocks);
}

/* ============================================================
 * 锁统计
 * ============================================================ */

void lock_get_stats(lock_manager_t *mgr, lock_stats_t *stats)
{
    if (!mgr || !stats) {
        return;
    }

    stats->lock_requests = atomic_load(&mgr->lock_requests);
    stats->lock_waits = atomic_load(&mgr->lock_waits);
    stats->deadlocks = atomic_load(&mgr->deadlocks);
    stats->lock_escalations = atomic_load(&mgr->lock_escalations);

    /* 统计当前活动锁数量 */
    uint64_t active = 0;
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        lock_entry_t *entry = mgr->lock_table[i];
        while (entry) {
            if (entry->exclusive_holder) {
                active++;
            }
            lock_request_t *req = entry->shared_holders;
            while (req) {
                active++;
                req = req->next;
            }
            entry = entry->hash_next;
        }
    }
    stats->active_locks = active;
}

/* ============================================================
 * 辅助函数
 * ============================================================ */

bool lock_compatible(lock_mode_t granted_mode, lock_mode_t requested_mode)
{
    return check_lock_compatibility(granted_mode, requested_mode);
}

const char *lock_mode_name(lock_mode_t mode)
{
    switch (mode) {
        case LOCK_MODE_S:   return "S";
        case LOCK_MODE_X:   return "X";
        case LOCK_MODE_IS:  return "IS";
        case LOCK_MODE_IX:  return "IX";
        case LOCK_MODE_SIX: return "SIX";
        default:            return "UNKNOWN";
    }
}

const char *lock_object_type_name(lock_object_type_t obj_type)
{
    switch (obj_type) {
        case LOCK_DATABASE: return "DATABASE";
        case LOCK_TABLE:    return "TABLE";
        case LOCK_PAGE:     return "PAGE";
        case LOCK_ROW:      return "ROW";
        default:            return "UNKNOWN";
    }
}

void lock_ctx_init(lock_ctx_t *ctx, lock_manager_t *mgr, txn_t *txn)
{
    if (!ctx) {
        return;
    }

    memset(ctx, 0, sizeof(lock_ctx_t));
    ctx->manager = mgr;
    ctx->txn = txn;
    ctx->held_locks = NULL;
    ctx->wait_depth = 0;
    ctx->marked = false;
    ctx->wait_start = 0;
}

void lock_ctx_cleanup(lock_ctx_t *ctx)
{
    if (!ctx) {
        return;
    }

    /* 释放事务持有的所有锁 */
    if (ctx->manager && ctx->txn) {
        lock_record_t *record = ctx->held_locks;
        while (record) {
            lock_record_t *next = record->next;
            lock_release(ctx->manager, ctx->txn,
                        record->obj_type, record->object_id,
                        record->mode);
            free(record);
            record = next;
        }
    }

    memset(ctx, 0, sizeof(lock_ctx_t));
}
