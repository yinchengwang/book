/**
 * @file predicate.c
 * @brief 谓词锁和 SSI 检测实现
 *
 * 参考 PostgreSQL: src/backend/storage/lmgr/predicate.c
 */

#include "db/storage/txn/predicate.h"
#include "db/storage/txn/mvcc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 谓词锁存储
 * ======================================================================== */

#define MAX_PREDICATE_LOCKS 4096
#define MAX_SIREAD_LOCKS 1024

/** 谓词锁表 */
static PredicateLockEntry g_predicate_locks[MAX_PREDICATE_LOCKS];
static int g_predicate_lock_count = 0;

/** SIREAD 锁表 */
static SireadLockEntry g_siread_locks[MAX_SIREAD_LOCKS];
static int g_siread_lock_count = 0;

/** SSI 状态 */
static SsiState g_ssi_state;

/** 谓词锁子系统是否初始化 */
static bool g_predicate_initialized = false;

/* ========================================================================
 * 初始化
 * ======================================================================== */

/**
 * @brief 初始化谓词锁子系统
 */
void predicate_lock_init(void)
{
    if (g_predicate_initialized) {
        return;
    }

    memset(g_predicate_locks, 0, sizeof(g_predicate_locks));
    memset(g_siread_locks, 0, sizeof(g_siread_locks));
    memset(&g_ssi_state, 0, sizeof(g_ssi_state));

    g_predicate_lock_count = 0;
    g_siread_lock_count = 0;
    g_predicate_initialized = true;
}

/**
 * @brief 清理谓词锁子系统
 */
void predicate_lock_shutdown(void)
{
    if (!g_predicate_initialized) {
        return;
    }

    memset(g_predicate_locks, 0, sizeof(g_predicate_locks));
    memset(g_siread_locks, 0, sizeof(g_siread_locks));
    memset(&g_ssi_state, 0, sizeof(g_ssi_state));

    g_predicate_lock_count = 0;
    g_siread_lock_count = 0;
    g_predicate_initialized = false;
}

/* ========================================================================
 * 谓词锁操作
 * ======================================================================== */

/**
 * @brief 比较两个锁标签是否相等
 */
static bool lock_tag_equals(const PredicateLockTag *a, const PredicateLockTag *b)
{
    if (a->relfilenode != b->relfilenode) return false;
    if (a->blocknum != b->blocknum) return false;
    if (a->offset != b->offset) return false;
    if (a->target != b->target) return false;
    return true;
}

/**
 * @brief 查找谓词锁条目
 */
static PredicateLockEntry *find_predicate_lock(const PredicateLockTag *tag)
{
    for (int i = 0; i < g_predicate_lock_count; i++) {
        if (lock_tag_equals(&g_predicate_locks[i].tag, tag)) {
            return &g_predicate_locks[i];
        }
    }
    return NULL;
}

/**
 * @brief 获取表级 SIREAD 锁
 */
bool predicate_lock_relation(uint32_t relfilenode, TransactionId xid)
{
    if (!g_predicate_initialized) {
        predicate_lock_init();
    }

    /* 检查是否已存在 */
    for (int i = 0; i < g_siread_lock_count; i++) {
        if (g_siread_locks[i].relfilenode == relfilenode &&
            g_siread_locks[i].xid == xid) {
            return true;  /* 已存在 */
        }
    }

    /* 添加新条目 */
    if (g_siread_lock_count >= MAX_SIREAD_LOCKS) {
        return false;  /* 表满 */
    }

    g_siread_locks[g_siread_lock_count].relfilenode = relfilenode;
    g_siread_locks[g_siread_lock_count].xid = xid;
    g_siread_lock_count++;

    return true;
}

/**
 * @brief 获取页面级谓词锁
 */
bool predicate_lock_page(uint32_t relfilenode, uint32_t blocknum,
                        TransactionId xid, predicate_lock_type_t lock_type)
{
    PredicateLockTag tag = {
        .relfilenode = relfilenode,
        .blocknum = blocknum,
        .offset = 0,
        .key_hash = 0,
        .target = PREDICATE_LOCK_PAGE
    };

    PredicateLockEntry *entry = find_predicate_lock(&tag);
    if (entry) {
        /* 更新锁类型 */
        if (lock_type == PREDICATE_LOCK_EXCLUSIVE) {
            entry->lock_type = PREDICATE_LOCK_EXCLUSIVE;
        }
        entry->holder_xid = xid;
        return true;
    }

    if (g_predicate_lock_count >= MAX_PREDICATE_LOCKS) {
        return false;
    }

    entry = &g_predicate_locks[g_predicate_lock_count++];
    entry->tag = tag;
    entry->holder_xid = xid;
    entry->lock_type = lock_type;
    entry->committed = false;

    return true;
}

/**
 * @brief 获取元组级谓词锁
 */
bool predicate_lock_tuple(uint32_t relfilenode, uint32_t blocknum,
                          uint32_t offset, TransactionId xid,
                          predicate_lock_type_t lock_type)
{
    PredicateLockTag tag = {
        .relfilenode = relfilenode,
        .blocknum = blocknum,
        .offset = offset,
        .key_hash = 0,
        .target = PREDICATE_LOCK_TUPLE
    };

    PredicateLockEntry *entry = find_predicate_lock(&tag);
    if (entry) {
        if (lock_type == PREDICATE_LOCK_EXCLUSIVE) {
            entry->lock_type = PREDICATE_LOCK_EXCLUSIVE;
        }
        entry->holder_xid = xid;
        return true;
    }

    if (g_predicate_lock_count >= MAX_PREDICATE_LOCKS) {
        return false;
    }

    entry = &g_predicate_locks[g_predicate_lock_count++];
    entry->tag = tag;
    entry->holder_xid = xid;
    entry->lock_type = lock_type;
    entry->committed = false;

    return true;
}

/**
 * @brief 获取键范围谓词锁
 */
bool predicate_lock_keyrange(uint32_t relfilenode, uint64_t key_hash,
                             TransactionId xid, predicate_lock_type_t lock_type)
{
    PredicateLockTag tag = {
        .relfilenode = relfilenode,
        .blocknum = 0,
        .offset = 0,
        .key_hash = key_hash,
        .target = PREDICATE_LOCK_KEYRANGE
    };

    PredicateLockEntry *entry = find_predicate_lock(&tag);
    if (entry) {
        if (lock_type == PREDICATE_LOCK_EXCLUSIVE) {
            entry->lock_type = PREDICATE_LOCK_EXCLUSIVE;
        }
        entry->holder_xid = xid;
        return true;
    }

    if (g_predicate_lock_count >= MAX_PREDICATE_LOCKS) {
        return false;
    }

    entry = &g_predicate_locks[g_predicate_lock_count++];
    entry->tag = tag;
    entry->holder_xid = xid;
    entry->lock_type = lock_type;
    entry->committed = false;

    return true;
}

/**
 * @brief 检查是否存在排他锁冲突
 */
bool predicate_has_exclusive_conflict(const PredicateLockTag *tag, TransactionId xid)
{
    PredicateLockEntry *entry = find_predicate_lock(tag);
    if (!entry) {
        return false;
    }

    /* 已有排他锁，与其他事务冲突 */
    if (entry->lock_type == PREDICATE_LOCK_EXCLUSIVE &&
        entry->holder_xid != xid) {
        return true;
    }

    return false;
}

/**
 * @brief 检查是否存在共享锁冲突
 */
bool predicate_has_shared_conflict(const PredicateLockTag *tag, TransactionId xid)
{
    PredicateLockEntry *entry = find_predicate_lock(tag);
    if (!entry) {
        return false;
    }

    /* 已有排他锁，任何其他事务都是冲突 */
    if (entry->lock_type == PREDICATE_LOCK_EXCLUSIVE &&
        entry->holder_xid != xid) {
        return true;
    }

    return false;
}

/* ========================================================================
 * SSI 检测
 * ======================================================================== */

/**
 * @brief 记录读写依赖：T1 写 A，T2 读 A -> T2 依赖于 T1
 */
void ssi_record_rw_depend(TransactionId writer_xid, TransactionId reader_xid)
{
    if (g_ssi_state.rw_depend_count >= 256) {
        return;  /* 表满 */
    }

    /* 检查是否已存在 */
    for (int i = 0; i < g_ssi_state.rw_depend_count; i++) {
        if (g_ssi_state.rw_depend[i] == writer_xid) {
            return;
        }
    }

    g_ssi_state.rw_depend[g_ssi_state.rw_depend_count++] = writer_xid;

    /*
     * 检查是否形成危险结构：
     * 如果存在 T1 -wr-> T2（reader_xid 写了 writer_xid 读过的数据）
     * 则形成 T1 -> T2 -> T1 环，导致序列化异常
     */
    for (int i = 0; i < g_ssi_state.wr_depend_count; i++) {
        if (g_ssi_state.wr_depend[i] == writer_xid) {
            /* 发现环！T1 -> reader_xid -> T1 */
            g_ssi_state.serialize_failure = true;
            return;
        }
    }
}

/**
 * @brief 记录写读依赖：T1 读 A，T2 写 A -> T2 依赖于 T1
 */
void ssi_record_wr_depend(TransactionId reader_xid, TransactionId writer_xid)
{
    if (g_ssi_state.wr_depend_count >= 256) {
        return;
    }

    /* 检查是否已存在 */
    for (int i = 0; i < g_ssi_state.wr_depend_count; i++) {
        if (g_ssi_state.wr_depend[i] == reader_xid) {
            return;
        }
    }

    g_ssi_state.wr_depend[g_ssi_state.wr_depend_count++] = reader_xid;

    /*
     * 检查是否形成危险结构：
     * 如果存在 T1 -rw-> T2（writer_xid 写了 reader_xid 读过的数据）
     * 则形成 T1 -> T2 -> T1 环
     */
    for (int i = 0; i < g_ssi_state.rw_depend_count; i++) {
        if (g_ssi_state.rw_depend[i] == reader_xid) {
            /* 发现环！ */
            g_ssi_state.serialize_failure = true;
            return;
        }
    }
}

/**
 * @brief 检测序列化异常
 *
 * SSI 通过检测依赖图中的环来检测序列化异常。
 * 当存在 T1 -rw-> T2 且 T1 -wr-> T2（或反之）时，
 * 两个事务不能以串行顺序执行。
 */
bool ssi_detect_anomaly(void)
{
    /* 检查是否存在环 */
    for (int i = 0; i < g_ssi_state.rw_depend_count; i++) {
        TransactionId writer = g_ssi_state.rw_depend[i];
        for (int j = 0; j < g_ssi_state.wr_depend_count; j++) {
            TransactionId reader = g_ssi_state.wr_depend[j];
            if (writer == reader) {
                return true;  /* 发现环 */
            }
        }
    }
    return false;
}

/**
 * @brief 获取 SSI 状态
 */
SsiState *ssi_get_state(void)
{
    return &g_ssi_state;
}

/**
 * @brief 重置 SSI 状态
 */
void ssi_reset(void)
{
    memset(&g_ssi_state, 0, sizeof(g_ssi_state));
}

/**
 * @brief 检查当前事务是否可以继续
 */
bool ssi_can_continue(void)
{
    return !g_ssi_state.serialize_failure;
}

/**
 * @brief 标记序列化失败
 */
void ssi_mark_failure(void)
{
    g_ssi_state.serialize_failure = true;
}

/* ========================================================================
 * 调试
 * ======================================================================== */

/**
 * @brief 打印谓词锁状态
 */
void predicate_lock_dump(void)
{
    printf("=== Predicate Locks ===\n");
    printf("Count: %d\n", g_predicate_lock_count);

    for (int i = 0; i < g_predicate_lock_count; i++) {
        PredicateLockEntry *e = &g_predicate_locks[i];
        printf("  [%d] rel=%u, block=%u, offset=%u, type=%s, holder=%u\n",
               i, e->tag.relfilenode, e->tag.blocknum, e->tag.offset,
               e->lock_type == PREDICATE_LOCK_EXCLUSIVE ? "X" : "S",
               e->holder_xid);
    }

    printf("\n=== SIREAD Locks ===\n");
    printf("Count: %d\n", g_siread_lock_count);

    for (int i = 0; i < g_siread_lock_count; i++) {
        printf("  [%d] rel=%u, xid=%u\n",
               i, g_siread_locks[i].relfilenode, g_siread_locks[i].xid);
    }
}

/**
 * @brief 打印 SSI 状态
 */
void ssi_dump(void)
{
    printf("=== SSI State ===\n");
    printf("RW Depend Count: %d\n", g_ssi_state.rw_depend_count);
    for (int i = 0; i < g_ssi_state.rw_depend_count; i++) {
        printf("  [%d] writer=%u\n", i, g_ssi_state.rw_depend[i]);
    }

    printf("WR Depend Count: %d\n", g_ssi_state.wr_depend_count);
    for (int i = 0; i < g_ssi_state.wr_depend_count; i++) {
        printf("  [%d] reader=%u\n", i, g_ssi_state.wr_depend[i]);
    }

    printf("Serialize Failure: %s\n",
           g_ssi_state.serialize_failure ? "YES" : "NO");
}
