/**
 * @file text.c
 * @brief 文本模型 CRUD + FTS5 检索
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdk/mmdb.h"
#include "sdk/mmdb_text.h"
#include "sdk/impl/mmdb_internal.h"
#include "sdk/impl/sqlite_backend.h"
#include "sdk/impl/filter_parser.h"
#include "text_fts5.c"  /* 包含 FTS5 初始化函数 */
#include "text_sql.c"   /* 包含 SQL 构造函数 */

/* Forward declaration */
int mmdb_text_fts5_init(sqlite3* sdb, const char* collection_name);
int mmdb_text_build_search_sql(const char* collection_name,
                                const char* query,
                                char** out_sql);

#include <sqlite3.h>

/* ------------------------------------------------------------------ */
/* 内部辅助：确保数据表存在                                              */
/* ------------------------------------------------------------------ */

static int ensure_text_table(mmdb_collection_t* c) {
    if (!c || c->model != MMDB_MODEL_TEXT) return MMDB_ERR_INVALID;

    /* 检查表是否已存在 */
    char check_sql[256];
    snprintf(check_sql, sizeof(check_sql),
        "SELECT 1 FROM sqlite_master WHERE type='table' AND name='mmdb_text_%s';",
        c->name);

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, check_sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    int exists = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    if (exists) return MMDB_OK;

    /* 创建文本数据表 */
    char ddl[512];
    snprintf(ddl, sizeof(ddl),
        "CREATE TABLE IF NOT EXISTS mmdb_text_%s ("
        "  rowid INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  id TEXT UNIQUE,"
        "  text TEXT NOT NULL,"
        "  metadata_json TEXT"
        ");",
        c->name);

    int rc = mmdb_sqlite_exec(c->sdb, ddl);
    if (rc != MMDB_OK) return rc;

    /* 创建 FTS5 虚表和触发器 */
    rc = mmdb_text_fts5_init(c->sdb, c->name);
    return rc;
}

/* ------------------------------------------------------------------ */
/* 公共 API                                                            */
/* ------------------------------------------------------------------ */

int mmdb_text_add(mmdb_collection_t* c, const mmdb_text_entry_t* entry) {
    return mmdb_text_add_batch(c, entry, 1);
}

int mmdb_text_add_batch(mmdb_collection_t* c,
                         const mmdb_text_entry_t* entries, size_t n) {
    if (!c || (!entries && n > 0)) return MMDB_ERR_INVALID;
    if (n == 0) return MMDB_OK;
    if (c->model != MMDB_MODEL_TEXT) return MMDB_ERR_INVALID;

    /* 确保表存在 */
    int rc = ensure_text_table(c);
    if (rc != MMDB_OK) return rc;

    /* 构建 INSERT SQL */
    char sql[256];
    snprintf(sql, sizeof(sql),
        "INSERT INTO mmdb_text_%s (id, text, metadata_json) "
        "VALUES (?, ?, ?);",
        c->name);

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    /* 写操作：获取写锁 */
    pthread_rwlock_wrlock(c->coll_lock);
    int err = MMDB_OK;
    for (size_t i = 0; i < n; i++) {
        const mmdb_text_entry_t* e = &entries[i];
        sqlite3_reset(stmt);
        sqlite3_clear_bindings(stmt);

        if (e->id) {
            mmdb_sqlite_bind_text(stmt, 1, e->id);
        } else {
            sqlite3_bind_null(stmt, 1);
        }
        mmdb_sqlite_bind_text(stmt, 2, e->text);
        if (e->metadata_json) {
            mmdb_sqlite_bind_text(stmt, 3, e->metadata_json);
        } else {
            sqlite3_bind_null(stmt, 3);
        }

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            err = MMDB_ERR_IO;
            break;
        }
    }
    sqlite3_finalize(stmt);
    pthread_rwlock_unlock(c->coll_lock);
    return err;
}

int mmdb_text_get(mmdb_collection_t* c, const char* id,
                   mmdb_text_entry_t* out) {
    if (!c || !id || !out) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_TEXT) return MMDB_ERR_INVALID;

    /* 确保表存在 */
    int rc = ensure_text_table(c);
    if (rc != MMDB_OK) return rc;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "SELECT id, text, metadata_json FROM mmdb_text_%s WHERE id = ?;",
        c->name);

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    mmdb_sqlite_bind_text(stmt, 1, id);

    /* 读操作：获取读锁（支持并发读） */
    pthread_rwlock_rdlock(c->coll_lock);
    rc = MMDB_ERR_NOT_FOUND;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* txt = (const char*)sqlite3_column_text(stmt, 1);
        const char* meta = (const char*)sqlite3_column_text(stmt, 2);
        out->id = NULL;  /* 调用方管理 id 内存 */
        out->text = txt ? strdup(txt) : NULL;
        out->metadata_json = meta ? strdup(meta) : NULL;
        rc = MMDB_OK;
    }
    sqlite3_finalize(stmt);
    pthread_rwlock_unlock(c->coll_lock);
    return rc;
}

int mmdb_text_delete(mmdb_collection_t* c, const char* id) {
    if (!c || !id) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_TEXT) return MMDB_ERR_INVALID;

    /* 确保表存在 */
    int rc = ensure_text_table(c);
    if (rc != MMDB_OK) return rc;

    char sql[256];
    snprintf(sql, sizeof(sql),
        "DELETE FROM mmdb_text_%s WHERE id = ?;",
        c->name);

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    if (!stmt) return MMDB_ERR_IO;

    mmdb_sqlite_bind_text(stmt, 1, id);

    /* 写操作：获取写锁（delete 会修改底层数据） */
    pthread_rwlock_wrlock(c->coll_lock);
    rc = (sqlite3_step(stmt) == SQLITE_DONE) ? MMDB_OK : MMDB_ERR_IO;
    sqlite3_finalize(stmt);
    pthread_rwlock_unlock(c->coll_lock);
    return rc;
}

int mmdb_text_search(mmdb_collection_t* c, const mmdb_text_query_t* q,
                      mmdb_result_t* out) {
    if (!c || !q || !q->query || !out) return MMDB_ERR_INVALID;
    if (c->model != MMDB_MODEL_TEXT) return MMDB_ERR_INVALID;

    memset(out, 0, sizeof(*out));

    /* 确保表存在 */
    int rc = ensure_text_table(c);
    if (rc != MMDB_OK) return rc;

    /* 构建搜索 SQL */
    char* sql = NULL;
    rc = mmdb_text_build_search_sql(c->name, q->query, &sql);
    if (rc != MMDB_OK) return rc;

    sqlite3_stmt* stmt = mmdb_sqlite_prepare(c->sdb, sql, NULL, 0);
    free(sql);
    if (!stmt) return MMDB_ERR_IO;

    /* 绑定搜索关键词 */
    mmdb_sqlite_bind_text(stmt, 1, q->query);

    /* 读操作：获取读锁（支持并发读） */
    pthread_rwlock_rdlock(c->coll_lock);

    /* 扫描结果 */
    size_t cap = 16;
    out->items = (mmdb_result_item_t*)calloc(cap, sizeof(mmdb_result_item_t));
    if (!out->items) {
        sqlite3_finalize(stmt);
        pthread_rwlock_unlock(c->coll_lock);
        return MMDB_ERR_NOMEM;
    }

    size_t top_k = q->top_k > 0 ? q->top_k : 100;
    while (sqlite3_step(stmt) == SQLITE_ROW && out->count < top_k) {
        if (out->count >= cap) {
            cap *= 2;
            out->items = (mmdb_result_item_t*)realloc(
                out->items, sizeof(mmdb_result_item_t) * cap);
            if (!out->items) {
                sqlite3_finalize(stmt);
                pthread_rwlock_unlock(c->coll_lock);
                return MMDB_ERR_NOMEM;
            }
        }

        mmdb_result_item_t* it = &out->items[out->count];
        const char* id = (const char*)sqlite3_column_text(stmt, 0);
        const char* txt = (const char*)sqlite3_column_text(stmt, 1);
        const char* meta = (const char*)sqlite3_column_text(stmt, 2);
        double rank = sqlite3_column_double(stmt, 3);

        it->id = id ? (uint8_t*)strdup(id) : NULL;
        it->id_len = id ? strlen(id) : 0;
        it->text = txt ? strdup(txt) : NULL;
        it->metadata_json = meta ? strdup(meta) : NULL;
        it->distance = (float)(-rank);  /* rank 越低越好，取负作为距离 */
        out->count++;
    }

    sqlite3_finalize(stmt);
    pthread_rwlock_unlock(c->coll_lock);
    return MMDB_OK;
}
