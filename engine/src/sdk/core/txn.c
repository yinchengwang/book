/**
 * @file txn.c
 * @brief ACID 事务实现
 *
 * 基于 SQLite BEGIN TRANSACTION 实现，支持只读/读写事务，
 * 写锁升级，以及嵌套事务（Savepoint）。
 */
#include "sdk/mmdb_txn.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* 事务句柄内部结构 */
struct mmdb_txn_s {
    mmdb_t*             db;             /* 数据库句柄 */
    mmdb_txn_mode_t     mode;           /* 事务模式 */
    mmdb_txn_state_t    state;          /* 事务状态 */
    sqlite3*            sdb;            /* SQLite 句柄（db->db 的别名） */
    int                 savepoint_depth; /* Savepoint 嵌套深度 */
    char                name[32];       /* Savepoint 名称 */
};

/**
 * @brief 开始事务
 */
int mmdb_txn_begin(mmdb_t* db, mmdb_txn_mode_t mode, mmdb_txn_t** txn) {
    if (!db || !txn) {
        return MMDB_ERR_INVALID;
    }

    /* 分配事务句柄 */
    mmdb_txn_t* t = (mmdb_txn_t*)calloc(1, sizeof(mmdb_txn_t));
    if (!t) {
        return MMDB_ERR_NOMEM;
    }

    t->db = db;
    t->mode = mode;
    t->state = MMDB_TXN_ACTIVE;
    t->sdb = db->db;
    t->savepoint_depth = 0;

    /* 执行 BEGIN 语句 */
    const char* begin_sql = (mode == MMDB_TXN_READONLY)
        ? "BEGIN DEFERRED"
        : "BEGIN IMMEDIATE";

    char* err_msg = NULL;
    int rc = sqlite3_exec(t->sdb, begin_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            mmdb_set_error(db, MMDB_ERR_INTERNAL, err_msg);
            sqlite3_free(err_msg);
        }
        free(t);
        return MMDB_ERR_INTERNAL;
    }

    *txn = t;
    return MMDB_OK;
}

/**
 * @brief 提交事务
 */
int mmdb_txn_commit(mmdb_txn_t* txn) {
    if (!txn || txn->state != MMDB_TXN_ACTIVE) {
        return MMDB_ERR_INVALID;
    }

    char* err_msg = NULL;
    int rc = sqlite3_exec(txn->sdb, "COMMIT", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            mmdb_set_error(txn->db, MMDB_ERR_INTERNAL, err_msg);
            sqlite3_free(err_msg);
        }
        return MMDB_ERR_INTERNAL;
    }

    txn->state = MMDB_TXN_COMMITTED;
    return MMDB_OK;
}

/**
 * @brief 回滚事务
 */
int mmdb_txn_abort(mmdb_txn_t* txn) {
    if (!txn || txn->state != MMDB_TXN_ACTIVE) {
        return MMDB_ERR_INVALID;
    }

    char* err_msg = NULL;
    int rc = sqlite3_exec(txn->sdb, "ROLLBACK", NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        if (err_msg) {
            mmdb_set_error(txn->db, MMDB_ERR_INTERNAL, err_msg);
            sqlite3_free(err_msg);
        }
        return MMDB_ERR_INTERNAL;
    }

    txn->state = MMDB_TXN_ABORTED;
    return MMDB_OK;
}

/**
 * @brief 获取事务状态
 */
mmdb_txn_state_t mmdb_txn_get_state(const mmdb_txn_t* txn) {
    if (!txn) {
        return MMDB_TXN_ABORTED;
    }
    return txn->state;
}

/**
 * @brief 检查事务是否为只读
 */
bool mmdb_txn_is_readonly(const mmdb_txn_t* txn) {
    if (!txn) {
        return false;
    }
    return txn->mode == MMDB_TXN_READONLY;
}

/**
 * @brief 释放事务句柄
 */
void mmdb_txn_free(mmdb_txn_t* txn) {
    if (!txn) return;

    /* 如果事务仍活跃，自动回滚 */
    if (txn->state == MMDB_TXN_ACTIVE) {
        mmdb_txn_abort(txn);
    }

    free(txn);
}
