/**
 * @file sqlite_backend.c
 * @brief SQLite 后端实现：连接管理、PRAGMA、Bootstrap、参数化执行
 */
#include "sdk/impl/sqlite_backend.h"

#include <stdio.h>
#include <string.h>

#include "sdk/mmdb_error.h"

/* ------------------------------------------------------------------ */
/* 打开与 PRAGMA 配置                                                  */
/* ------------------------------------------------------------------ */

int mmdb_sqlite_open(const char* path, const mmdb_options_t* opts,
                     sqlite3** out, char* err_buf, size_t err_buf_len) {
    if (!path || !out) {
        if (err_buf && err_buf_len > 0) {
            snprintf(err_buf, err_buf_len, "path/out NULL");
        }
        return MMDB_ERR_INVALID;
    }

    sqlite3* db = NULL;
    int rc = sqlite3_open_v2(path, &db,
                            SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                                SQLITE_OPEN_FULLMUTEX,
                            NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_buf_len > 0) {
            snprintf(err_buf, err_buf_len, "sqlite3_open: %s",
                     db ? sqlite3_errmsg(db) : "out of memory");
        }
        if (db) sqlite3_close(db);
        return MMDB_ERR_IO;
    }

    /* busy timeout */
    int busy_ms = opts ? opts->busy_timeout_ms : 5000;
    char pragma[128];
    snprintf(pragma, sizeof(pragma), "PRAGMA busy_timeout=%d;", busy_ms);
    sqlite3_exec(db, pragma, NULL, NULL, NULL);

    /* WAL 模式 */
    int want_wal = opts ? opts->enable_wal : 1;
    if (want_wal) {
        sqlite3_exec(db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
        sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);
        sqlite3_exec(db, "PRAGMA journal_size_limit=4096;", NULL, NULL, NULL);
    }

    /* cache size（单位：KB，负值 = KiB） */
    int cache_kb = opts ? opts->cache_size_kb : 16384;
    if (cache_kb > 0) {
        snprintf(pragma, sizeof(pragma), "PRAGMA cache_size=-%d;", cache_kb);
        sqlite3_exec(db, pragma, NULL, NULL, NULL);
    }

    /* 外键约束 */
    sqlite3_exec(db, "PRAGMA foreign_keys=ON;", NULL, NULL, NULL);

    *out = db;
    return MMDB_OK;
}

void mmdb_sqlite_close(sqlite3* db) {
    if (db) sqlite3_close(db);
}

/* ------------------------------------------------------------------ */
/* Bootstrap：创建系统表                                               */
/* ------------------------------------------------------------------ */

int mmdb_sqlite_bootstrap(sqlite3* db) {
    static const char* kSchema =
        "CREATE TABLE IF NOT EXISTS mmdb_collections ("
        "  name TEXT PRIMARY KEY,"
        "  model INTEGER NOT NULL,"
        "  schema_json TEXT NOT NULL,"
        "  vector_dim INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL"
        ");"
        "CREATE TABLE IF NOT EXISTS mmdb_schema_version ("
        "  version INTEGER PRIMARY KEY"
        ");"
        "INSERT OR IGNORE INTO mmdb_schema_version(version) VALUES (1);";

    char* err = NULL;
    int rc = sqlite3_exec(db, kSchema, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return MMDB_ERR_CORRUPT;
    }
    return MMDB_OK;
}

/* ------------------------------------------------------------------ */
/* 事务封装                                                            */
/* ------------------------------------------------------------------ */

int mmdb_sqlite_begin(sqlite3* db) {
    char* err = NULL;
    int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return MMDB_ERR_IO;
    }
    return MMDB_OK;
}

int mmdb_sqlite_commit(sqlite3* db) {
    char* err = NULL;
    int rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return MMDB_ERR_IO;
    }
    return MMDB_OK;
}

int mmdb_sqlite_rollback(sqlite3* db) {
    char* err = NULL;
    int rc = sqlite3_exec(db, "ROLLBACK;", NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return MMDB_ERR_INTERNAL;
    }
    return MMDB_OK;
}

/* ------------------------------------------------------------------ */
/* 单条执行                                                            */
/* ------------------------------------------------------------------ */

int mmdb_sqlite_exec(sqlite3* db, const char* sql) {
    if (!db || !sql) return MMDB_ERR_INVALID;
    char* err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        if (err) sqlite3_free(err);
        return MMDB_ERR_IO;
    }
    return MMDB_OK;
}

/* ------------------------------------------------------------------ */
/* 参数化执行辅助                                                      */
/* ------------------------------------------------------------------ */

sqlite3_stmt* mmdb_sqlite_prepare(sqlite3* db, const char* sql, char* err_buf,
                                   size_t err_buf_len) {
    sqlite3_stmt* stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        if (err_buf && err_buf_len > 0) {
            snprintf(err_buf, err_buf_len, "prepare: %s",
                     sqlite3_errmsg(db));
        }
        return NULL;
    }
    return stmt;
}

int mmdb_sqlite_bind_int(sqlite3_stmt* stmt, int idx, int64_t v) {
    return sqlite3_bind_int64(stmt, idx, v) == SQLITE_OK ? MMDB_OK
                                                        : MMDB_ERR_INVALID;
}

int mmdb_sqlite_bind_text(sqlite3_stmt* stmt, int idx, const char* s) {
    /* SQLITE_TRANSIENT：让 SQLite 复制字符串，避免悬空指针 */
    return sqlite3_bind_text(stmt, idx, s, -1, SQLITE_TRANSIENT) == SQLITE_OK
               ? MMDB_OK
               : MMDB_ERR_INVALID;
}

int mmdb_sqlite_bind_blob(sqlite3_stmt* stmt, int idx, const void* p, size_t n) {
    return sqlite3_bind_blob(stmt, idx, p, (int)n, SQLITE_TRANSIENT) ==
                   SQLITE_OK
               ? MMDB_OK
               : MMDB_ERR_INVALID;
}

/* 优化版本：使用 SQLITE_STATIC 避免 malloc 拷贝（调用者必须保证数据在 step 前有效） */
int mmdb_sqlite_bind_blob_static(sqlite3_stmt* stmt, int idx, const void* p, size_t n) {
    return sqlite3_bind_blob(stmt, idx, p, (int)n, SQLITE_STATIC) ==
                   SQLITE_OK
               ? MMDB_OK
               : MMDB_ERR_INVALID;
}

int mmdb_sqlite_bind_double(sqlite3_stmt* stmt, int idx, double v) {
    return sqlite3_bind_double(stmt, idx, v) == SQLITE_OK ? MMDB_OK
                                                         : MMDB_ERR_INVALID;
}