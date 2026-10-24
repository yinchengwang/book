/**
 * @file text_sql.c
 * @brief 文本 SQL 构造
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sdk/mmdb.h"
#include "sdk/impl/mmdb_internal.h"

#define TEXT_SQL_BUF 4096

/**
 * @brief 构建 FTS5 搜索 SQL
 * @param collection_name collection 名称（用于表名 mmdb_text_<name>）
 * @param query 搜索关键词
 * @param out_sql 输出的 SQL 字符串（调用方需 free）
 * @return MMDB_OK 成功
 */
int mmdb_text_build_search_sql(const char* collection_name,
                                const char* query,
                                char** out_sql) {
    if (!collection_name || !out_sql) return MMDB_ERR_INVALID;
    *out_sql = NULL;

    char* sql = (char*)malloc(TEXT_SQL_BUF);
    if (!sql) return MMDB_ERR_NOMEM;

    snprintf(sql, TEXT_SQL_BUF,
        "SELECT t.id, t.text, t.metadata_json, "
        "rank FROM mmdb_text_fts_%s f "
        "JOIN mmdb_text_%s t ON t.rowid = f.rowid "
        "WHERE mmdb_text_fts_%s MATCH ? "
        "ORDER BY rank LIMIT 100",
        collection_name, collection_name, collection_name);

    *out_sql = sql;
    return MMDB_OK;
}
