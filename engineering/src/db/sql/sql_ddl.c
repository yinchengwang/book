/**
 * @file sql_ddl.c
 * @brief DDL 执行器实现
 *
 * 实现 ALTER TABLE 语句的执行，通过 Catalog API 修改表元数据。
 */
#include "db/sql/sql_ddl.h"
#include "db/catalog.h"
#include "db/log.h"
#include <stdlib.h>
#include <string.h>

/* 类型名称 → type_oid 映射（简化版） */
static Oid type_name_to_oid(const char *type_name) {
    if (!type_name) return 0;
    if (strcasecmp(type_name, "int") == 0 || strcasecmp(type_name, "integer") == 0) return 1;
    if (strcasecmp(type_name, "bigint") == 0 || strcasecmp(type_name, "int8") == 0) return 2;
    if (strcasecmp(type_name, "varchar") == 0 || strcasecmp(type_name, "text") == 0) return 3;
    if (strcasecmp(type_name, "boolean") == 0 || strcasecmp(type_name, "bool") == 0) return 4;
    if (strcasecmp(type_name, "float") == 0 || strcasecmp(type_name, "real") == 0) return 5;
    if (strcasecmp(type_name, "double") == 0) return 6;
    if (strcasecmp(type_name, "date") == 0) return 7;
    if (strcasecmp(type_name, "timestamp") == 0) return 8;
    return 0; /* 未知类型 */
}

int exec_alter_table(AlterTableStmt *stmt, void *db) {
    if (!stmt || !stmt->relation) {
        log_error("exec_alter_table: 参数无效");
        return -1;
    }

    (void)db; /* catalog 操作不直接依赖 db 句柄 */

    /* 1. 查找表 */
    Oid table_oid = catalog_lookup_table(stmt->relation);
    if (table_oid == InvalidOid) {
        log_error("表不存在: %s", stmt->relation);
        return -1;
    }

    /* 2. 遍历命令 */
    for (int i = 0; i < stmt->num_cmds; i++) {
        AlterTableCmd *cmd = stmt->cmds[i];
        if (!cmd) continue;

        switch (cmd->subtype) {
            case AT_AddColumn: {
                if (!cmd->name) {
                    log_error("ADD COLUMN: 缺少列名");
                    return -1;
                }
                column_def_t col_def;
                memset(&col_def, 0, sizeof(col_def));
                strncpy(col_def.name, cmd->name, NAMEDATALEN - 1);
                col_def.type_oid = type_name_to_oid(cmd->type_name);
                col_def.not_null = cmd->not_null;
                col_def.has_default = (cmd->default_expr != NULL);

                if (catalog_add_column(table_oid, &col_def) != CATALOG_SUCCESS) {
                    log_error("ADD COLUMN 失败: '%s'", cmd->name);
                    return -1;
                }
                log_info("ADD COLUMN %s 到 %s", cmd->name, stmt->relation);
                break;
            }

            case AT_DropColumn: {
                if (!cmd->name) {
                    log_error("DROP COLUMN: 缺少列名");
                    return -1;
                }
                if (catalog_drop_column(table_oid, cmd->name) != CATALOG_SUCCESS) {
                    log_error("DROP COLUMN 失败: '%s'", cmd->name);
                    return -1;
                }
                log_info("DROP COLUMN %s 从 %s", cmd->name, stmt->relation);
                break;
            }

            case AT_AlterColumnType: {
                if (!cmd->name || !cmd->type_name) {
                    log_error("ALTER COLUMN TYPE: 缺少列名或类型");
                    return -1;
                }
                /* 获取现有列信息 */
                int ncols = 0;
                column_info_t *cols = catalog_get_columns(table_oid, &ncols);
                if (!cols) {
                    log_error("ALTER COLUMN TYPE: 无法获取列信息");
                    return -1;
                }

                int found = 0;
                for (int j = 0; j < ncols; j++) {
                    if (strcmp(cols[j].name, cmd->name) == 0 && !cols[j].is_dropped) {
                        /* 简化方案：DROP + ADD */
                        catalog_drop_column(table_oid, cmd->name);
                        column_def_t col_def;
                        memset(&col_def, 0, sizeof(col_def));
                        strncpy(col_def.name, cmd->name, NAMEDATALEN - 1);
                        col_def.type_oid = type_name_to_oid(cmd->type_name);
                        catalog_add_column(table_oid, &col_def);
                        found = 1;
                        break;
                    }
                }
                catalog_free_columns(cols);

                if (!found) {
                    log_error("ALTER COLUMN TYPE: 列 '%s' 不存在", cmd->name);
                    return -1;
                }
                log_info("ALTER COLUMN TYPE %s -> %s 在 %s", cmd->name, cmd->type_name, stmt->relation);
                break;
            }

            case AT_RenameColumn: {
                if (!cmd->name || !cmd->new_name) {
                    log_error("RENAME COLUMN: 缺少列名");
                    return -1;
                }
                /* Catalog 没有直接的 rename_column API */
                /* 使用内存中修改列名的方式（临时方案） */
                int ncols = 0;
                column_info_t *cols = catalog_get_columns(table_oid, &ncols);
                if (!cols) {
                    log_error("RENAME COLUMN: 无法获取列信息");
                    return -1;
                }

                int found = 0;
                for (int j = 0; j < ncols; j++) {
                    if (strcmp(cols[j].name, cmd->name) == 0 && !cols[j].is_dropped) {
                        strncpy(cols[j].name, cmd->new_name, NAMEDATALEN - 1);
                        found = 1;
                        break;
                    }
                }
                catalog_free_columns(cols);

                if (!found) {
                    log_error("RENAME COLUMN: 列 '%s' 不存在", cmd->name);
                    return -1;
                }
                log_warn("RENAME COLUMN %s -> %s 在 %s (内存更新，持久化需要 catalog_rename_column API)",
                         cmd->name, cmd->new_name, stmt->relation);
                break;
            }

            default:
                log_error("未知的 ALTER 操作: %d", cmd->subtype);
                return -1;
        }
    }

    /* 3. 使表元数据缓存失效 */
    catalog_invalidate_table(table_oid);

    return 0;
}