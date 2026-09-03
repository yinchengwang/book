/**
 * @file sqlite_backend.h
 * @brief SQLite 后端封装（内部接口）
 */
#ifndef SDK_IMPL_SQLITE_BACKEND_H
#define SDK_IMPL_SQLITE_BACKEND_H

#include "sdk/impl/mmdb_internal.h"
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 打开/创建 SQLite 数据库，应用 PRAGMA 与初始化 */
int mmdb_sqlite_open(const char* path, const mmdb_options_t* opts,
                     sqlite3** out, char* err_buf, size_t err_buf_len);

/* 关闭数据库（若仍打开） */
void mmdb_sqlite_close(sqlite3* db);

/* 创建系统表 mmdb_collections、mmdb_schema（幂等） */
int mmdb_sqlite_bootstrap(sqlite3* db);

/* 事务封装（错误自动回滚） */
int mmdb_sqlite_begin(sqlite3* db);
int mmdb_sqlite_commit(sqlite3* db);
int mmdb_sqlite_rollback(sqlite3* db);

/* 执行无返回结果的 SQL（如 INSERT/UPDATE/DELETE/CREATE） */
int mmdb_sqlite_exec(sqlite3* db, const char* sql);

/* 准备参数化语句（出错时返回 NULL 且通过 sqlite3_errmsg 写入 err） */
sqlite3_stmt* mmdb_sqlite_prepare(sqlite3* db, const char* sql, char* err_buf,
                                   size_t err_buf_len);

/* 绑定 int/text/blob 辅助函数（错误返回非 0） */
int mmdb_sqlite_bind_int(sqlite3_stmt* stmt, int idx, int64_t v);
int mmdb_sqlite_bind_text(sqlite3_stmt* stmt, int idx, const char* s);
int mmdb_sqlite_bind_blob(sqlite3_stmt* stmt, int idx, const void* p, size_t n);
int mmdb_sqlite_bind_blob_static(sqlite3_stmt* stmt, int idx, const void* p, size_t n);
int mmdb_sqlite_bind_double(sqlite3_stmt* stmt, int idx, double v);

#ifdef __cplusplus
}
#endif

#endif /* SDK_IMPL_SQLITE_BACKEND_H */