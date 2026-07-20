#ifndef TODO_DB_H
#define TODO_DB_H

#include <stdint.h>
#include <stddef.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 打开/创建数据库，path 为 .db 文件路径 */
int todo_db_open(const char *path);

/* 关闭数据库 */
void todo_db_close(void);

/* 从 JSON 文件迁移数据到 SQLite（迁移完成后重命名 JSON） */
int todo_db_migrate_from_json(const char *json_path);

/* 获取 SQLite 句柄（供其他模块使用） */
sqlite3 *todo_db_handle(void);

/* 元数据读写 */
int64_t todo_db_get_meta_int64(const char *key);
int todo_db_set_meta_int64(const char *key, int64_t value);

/* 检查表是否存在 */
int todo_db_table_exists(const char *table_name);

#ifdef __cplusplus
}
#endif

#endif /* TODO_DB_H */