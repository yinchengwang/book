/**
 * @file text_fts5.c
 * @brief FTS5 全文检索封装（SQL 触发器自动维护索引）
 */
#include "sdk/mmdb.h"
#include "sdk/impl/sqlite_backend.h"
#include "sdk/impl/mmdb_internal.h"

#include <stdio.h>
#include <string.h>

/**
 * @brief 为文本 collection 创建 FTS5 虚表和触发器
 * @param sdb SQLite 句柄
 * @param collection_name collection 名称
 * @return MMDB_OK 成功
 */
int mmdb_text_fts5_init(sqlite3* sdb, const char* collection_name) {
    if (!sdb || !collection_name) return MMDB_ERR_INVALID;

    char sql[1024];

    /* 创建 FTS5 虚表 */
    snprintf(sql, sizeof(sql),
        "CREATE VIRTUAL TABLE IF NOT EXISTS mmdb_text_fts_%s USING fts5("
        "  text, content=mmdb_text_%s, content_rowid=rowid"
        ");",
        collection_name, collection_name);

    if (mmdb_sqlite_exec(sdb, sql) != MMDB_OK) return MMDB_ERR_IO;

    /* 创建触发器：插入时同步 FTS 索引 */
    snprintf(sql, sizeof(sql),
        "CREATE TRIGGER IF NOT EXISTS mmdb_text_ai_%s AFTER INSERT ON mmdb_text_%s BEGIN "
        "  INSERT INTO mmdb_text_fts_%s(rowid, text) VALUES (new.rowid, new.text);"
        "END;",
        collection_name, collection_name, collection_name);
    if (mmdb_sqlite_exec(sdb, sql) != MMDB_OK) return MMDB_ERR_IO;

    /* 创建触发器：删除时同步 FTS 索引 */
    snprintf(sql, sizeof(sql),
        "CREATE TRIGGER IF NOT EXISTS mmdb_text_ad_%s AFTER DELETE ON mmdb_text_%s BEGIN "
        "  INSERT INTO mmdb_text_fts_%s(mmdb_text_fts_%s, rowid, text) "
        "VALUES('delete', old.rowid, old.text);"
        "END;",
        collection_name, collection_name, collection_name, collection_name);
    if (mmdb_sqlite_exec(sdb, sql) != MMDB_OK) return MMDB_ERR_IO;

    /* 创建触发器：更新时同步 FTS 索引 */
    snprintf(sql, sizeof(sql),
        "CREATE TRIGGER IF NOT EXISTS mmdb_text_au_%s AFTER UPDATE ON mmdb_text_%s BEGIN "
        "  INSERT INTO mmdb_text_fts_%s(mmdb_text_fts_%s, rowid, text) "
        "VALUES('delete', old.rowid, old.text);"
        "  INSERT INTO mmdb_text_fts_%s(rowid, text) VALUES (new.rowid, new.text);"
        "END;",
        collection_name, collection_name, collection_name, collection_name, collection_name);
    if (mmdb_sqlite_exec(sdb, sql) != MMDB_OK) return MMDB_ERR_IO;

    return MMDB_OK;
}
