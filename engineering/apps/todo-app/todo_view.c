/* ============================================================
 * todo_view.c - 视图系统实现
 *
 * 飞书多维表格风格视图：表格/看板/日历/甘特图
 * ============================================================ */

#include "todo_view.h"
#include "todo_db.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

/* 全局数据库上下文（由 todo_db.c 提供） */
extern sqlite3 *g_db;

/* 前向声明 */
static void view_clear_default_except(int64_t except_id);

/* ============================================================
 * 视图类型转换
 * ============================================================ */

const char *view_type_to_string(view_type_t type) {
    switch (type) {
        case VIEW_TYPE_TABLE:    return "table";
        case VIEW_TYPE_BOARD:    return "board";
        case VIEW_TYPE_CALENDAR: return "calendar";
        case VIEW_TYPE_GANTT:    return "gantt";
        default:                 return "table";
    }
}

view_type_t view_type_from_string(const char *str) {
    if (str == NULL) return VIEW_TYPE_TABLE;
    if (strcmp(str, "board") == 0)    return VIEW_TYPE_BOARD;
    if (strcmp(str, "calendar") == 0) return VIEW_TYPE_CALENDAR;
    if (strcmp(str, "gantt") == 0)    return VIEW_TYPE_GANTT;
    return VIEW_TYPE_TABLE;
}

/* ============================================================
 * 视图 CRUD
 * ============================================================ */

int view_create(const char *name, view_type_t type, const char *config, int is_default, int64_t *out_id) {
    if (g_db == NULL || name == NULL) return -1;

    char sql[512];
    snprintf(sql, sizeof(sql),
        "INSERT INTO views (name, type, config, is_default, sort_order, created_at) "
        "VALUES (?, ?, ?, ?, "
        "(SELECT COALESCE(MAX(sort_order), 0) + 1 FROM views), "
        "unixepoch())");

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, view_type_to_string(type), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, config ? config : "{}", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, is_default ? 1 : 0);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return -1;

    *out_id = sqlite3_last_insert_rowid(g_db);

    /* 如果是默认视图，取消其他默认 */
    if (is_default) {
        view_clear_default_except(*out_id);
    }

    return 0;
}

int view_get(int64_t id, view_t *out) {
    if (g_db == NULL || out == NULL) return -1;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT id, name, type, config, is_default, sort_order, created_at "
        "FROM views WHERE id = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);

    if (rc == SQLITE_ROW) {
        out->id = sqlite3_column_int64(stmt, 0);
        strncpy(out->name, (const char *)sqlite3_column_text(stmt, 1), sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        out->type = view_type_from_string((const char *)sqlite3_column_text(stmt, 2));
        strncpy(out->config, (const char *)sqlite3_column_text(stmt, 3), sizeof(out->config) - 1);
        out->config[sizeof(out->config) - 1] = '\0';
        out->is_default = sqlite3_column_int(stmt, 4);
        out->sort_order = sqlite3_column_int(stmt, 5);
        out->created_at = sqlite3_column_int64(stmt, 6);
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return -1;
}

int view_update(int64_t id, const char *name, const char *config, int *is_default) {
    if (g_db == NULL) return -1;

    /* 构建动态 SQL，避免 NULL 绑定问题 */
    char sql[512];
    char set_clause[256] = "";
    int param_count = 0;

    if (name != NULL) {
        strcat(set_clause, "name = ?, ");
        param_count++;
    }
    if (config != NULL) {
        strcat(set_clause, "config = ?, ");
        param_count++;
    }
    if (is_default != NULL) {
        strcat(set_clause, "is_default = ?, ");
        param_count++;
    }

    /* 移除末尾的 ", " */
    size_t len = strlen(set_clause);
    if (len > 2) set_clause[len - 2] = '\0';

    if (len == 0) return 0;  /* 无更新字段 */

    snprintf(sql, sizeof(sql), "UPDATE views SET %s WHERE id = ?", set_clause);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    int idx = 1;
    if (name != NULL)     sqlite3_bind_text(stmt, idx++, name, -1, SQLITE_TRANSIENT);
    if (config != NULL)   sqlite3_bind_text(stmt, idx++, config, -1, SQLITE_TRANSIENT);
    if (is_default != NULL) sqlite3_bind_int(stmt, idx++, *is_default ? 1 : 0);
    sqlite3_bind_int64(stmt, idx, id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) return -1;

    /* 如果设为默认，取消其他默认 */
    if (is_default != NULL && *is_default) {
        view_clear_default_except(id);
    }

    return 0;
}

int view_delete(int64_t id) {
    if (g_db == NULL) return -1;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, "DELETE FROM views WHERE id = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int view_list(view_t **out, int *out_count) {
    if (g_db == NULL || out == NULL || out_count == NULL) return -1;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT id, name, type, config, is_default, sort_order, created_at "
        "FROM views ORDER BY sort_order ASC", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    int capacity = 16;
    int count = 0;
    view_t *list = (view_t *)malloc(sizeof(view_t) * capacity);
    if (list == NULL) {
        sqlite3_finalize(stmt);
        return -1;
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (count >= capacity) {
            capacity *= 2;
            view_t *new_list = (view_t *)realloc(list, sizeof(view_t) * capacity);
            if (new_list == NULL) {
                free(list);
                sqlite3_finalize(stmt);
                return -1;
            }
            list = new_list;
        }

        list[count].id = sqlite3_column_int64(stmt, 0);
        strncpy(list[count].name, (const char *)sqlite3_column_text(stmt, 1), sizeof(list[count].name) - 1);
        list[count].name[sizeof(list[count].name) - 1] = '\0';
        list[count].type = view_type_from_string((const char *)sqlite3_column_text(stmt, 2));
        strncpy(list[count].config, (const char *)sqlite3_column_text(stmt, 3), sizeof(list[count].config) - 1);
        list[count].config[sizeof(list[count].config) - 1] = '\0';
        list[count].is_default = sqlite3_column_int(stmt, 4);
        list[count].sort_order = sqlite3_column_int(stmt, 5);
        list[count].created_at = sqlite3_column_int64(stmt, 6);
        count++;
    }

    sqlite3_finalize(stmt);

    *out = list;
    *out_count = count;
    return 0;
}

int view_set_default(int64_t id) {
    view_clear_default_except(id);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, "UPDATE views SET is_default = 1 WHERE id = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int view_get_default(view_t *out) {
    if (g_db == NULL || out == NULL) return -1;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db,
        "SELECT id, name, type, config, is_default, sort_order, created_at "
        "FROM views WHERE is_default = 1 LIMIT 1", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        out->id = sqlite3_column_int64(stmt, 0);
        strncpy(out->name, (const char *)sqlite3_column_text(stmt, 1), sizeof(out->name) - 1);
        out->name[sizeof(out->name) - 1] = '\0';
        out->type = view_type_from_string((const char *)sqlite3_column_text(stmt, 2));
        strncpy(out->config, (const char *)sqlite3_column_text(stmt, 3), sizeof(out->config) - 1);
        out->config[sizeof(out->config) - 1] = '\0';
        out->is_default = sqlite3_column_int(stmt, 4);
        out->sort_order = sqlite3_column_int(stmt, 5);
        out->created_at = sqlite3_column_int64(stmt, 6);
        sqlite3_finalize(stmt);
        return 0;
    }

    sqlite3_finalize(stmt);
    return -1;
}

int view_update_sort(int64_t id, int sort_order) {
    if (g_db == NULL) return -1;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(g_db, "UPDATE views SET sort_order = ? WHERE id = ?", -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, sort_order);
    sqlite3_bind_int64(stmt, 2, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

void view_free_list(view_t *list, int count) {
    (void)count;
    free(list);
}

/* 内部函数：清除除指定 ID 外的所有默认标记 */
static void view_clear_default_except(int64_t except_id) {
    if (g_db == NULL) return;

    char sql[128];
    snprintf(sql, sizeof(sql), "UPDATE views SET is_default = 0 WHERE id != %lld", (long long)except_id);

    sqlite3_exec(g_db, sql, NULL, NULL, NULL);
}
