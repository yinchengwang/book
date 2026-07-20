#include "todo_field.h"
#include "todo_db.h"
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 工具函数
 * ============================================================ */
static int64_t now_ts(void) {
    return (int64_t)time(NULL);
}

int field_type_from_string(const char *type_str) {
    if (strcmp(type_str, "text") == 0)          return FIELD_TYPE_TEXT;
    if (strcmp(type_str, "number") == 0)        return FIELD_TYPE_NUMBER;
    if (strcmp(type_str, "single_select") == 0) return FIELD_TYPE_SINGLE_SELECT;
    if (strcmp(type_str, "multi_select") == 0)  return FIELD_TYPE_MULTI_SELECT;
    if (strcmp(type_str, "date") == 0)          return FIELD_TYPE_DATE;
    if (strcmp(type_str, "datetime") == 0)      return FIELD_TYPE_DATETIME;
    if (strcmp(type_str, "user") == 0)          return FIELD_TYPE_USER;
    if (strcmp(type_str, "attachment") == 0)    return FIELD_TYPE_ATTACHMENT;
    if (strcmp(type_str, "formula") == 0)       return FIELD_TYPE_FORMULA;
    if (strcmp(type_str, "link") == 0)          return FIELD_TYPE_LINK;
    return FIELD_TYPE_TEXT;
}

const char *field_type_to_string(int type) {
    switch (type) {
        case FIELD_TYPE_TEXT:          return "text";
        case FIELD_TYPE_NUMBER:        return "number";
        case FIELD_TYPE_SINGLE_SELECT: return "single_select";
        case FIELD_TYPE_MULTI_SELECT:  return "multi_select";
        case FIELD_TYPE_DATE:          return "date";
        case FIELD_TYPE_DATETIME:      return "datetime";
        case FIELD_TYPE_USER:          return "user";
        case FIELD_TYPE_ATTACHMENT:    return "attachment";
        case FIELD_TYPE_FORMULA:       return "formula";
        case FIELD_TYPE_LINK:          return "link";
        default:                       return "text";
    }
}

/* ============================================================
 * 字段定义 CRUD
 * ============================================================ */

int field_def_create(const field_def_t *field, int64_t *out_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    const char *sql = "INSERT INTO fields_def (name, type, options, built_in, "
                      "ref_table, ref_field, formula, sort_order, created_at) "
                      "VALUES (?, ?, ?, 0, ?, ?, ?, ?, ?)";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "[field] prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    sqlite3_bind_text(stmt, 1, field->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, field_type_to_string(field->type), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, field->options, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, field->ref_table, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, field->ref_field, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, field->formula, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, field->sort_order);
    sqlite3_bind_int64(stmt, 8, now_ts());

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_DONE) {
        *out_id = sqlite3_last_insert_rowid(db);
        sqlite3_finalize(stmt);
        return 0;
    }

    fprintf(stderr, "[field] create error: %s\n", sqlite3_errmsg(db));
    sqlite3_finalize(stmt);
    return -1;
}

int field_def_get(int64_t id, field_def_t *field) {
    sqlite3 *db = todo_db_handle();
    if (!db || !field) return -1;

    const char *sql = "SELECT id, name, type, options, built_in, "
                      "ref_table, ref_field, formula, sort_order, created_at "
                      "FROM fields_def WHERE id = ?";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    field->id = sqlite3_column_int64(stmt, 0);
    snprintf(field->name, sizeof(field->name), "%s",
             (const char *)sqlite3_column_text(stmt, 1));
    field->type = field_type_from_string(
        (const char *)sqlite3_column_text(stmt, 2));
    snprintf(field->options, sizeof(field->options), "%s",
             (const char *)sqlite3_column_text(stmt, 3));
    field->built_in = sqlite3_column_int(stmt, 4);
    snprintf(field->ref_table, sizeof(field->ref_table), "%s",
             (const char *)sqlite3_column_text(stmt, 5));
    snprintf(field->ref_field, sizeof(field->ref_field), "%s",
             (const char *)sqlite3_column_text(stmt, 6));
    snprintf(field->formula, sizeof(field->formula), "%s",
             (const char *)sqlite3_column_text(stmt, 7));
    field->sort_order = sqlite3_column_int(stmt, 8);
    field->created_at = sqlite3_column_int64(stmt, 9);

    sqlite3_finalize(stmt);
    return 0;
}

int field_def_update(const field_def_t *field) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    /* 内置字段不允许改类型 */
    field_def_t old;
    if (field_def_get(field->id, &old) == 0 && old.built_in) {
        if (field->type != old.type) {
            fprintf(stderr, "[field] 内置字段不可修改类型\n");
            return -1;
        }
    }

    const char *sql = "UPDATE fields_def SET name=?, options=?, "
                      "ref_table=?, ref_field=?, formula=?, sort_order=? "
                      "WHERE id=?";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, field->name, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, field->options, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, field->ref_table, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, field->ref_field, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, field->formula, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 6, field->sort_order);
    sqlite3_bind_int64(stmt, 7, field->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int field_def_delete(int64_t id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    /* 内置字段不可删除 */
    field_def_t field;
    if (field_def_get(id, &field) == 0 && field.built_in) {
        fprintf(stderr, "[field] 内置字段不可删除\n");
        return -1;
    }

    /* 先删字段值（外键级联已处理，显式删除更安全） */
    const char *del_values = "DELETE FROM field_values WHERE field_id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, del_values, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    /* 删除字段定义 */
    const char *sql = "DELETE FROM fields_def WHERE id=?";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE && sqlite3_changes(db) > 0) ? 0 : -1;
}

int field_def_list(field_def_t **fields, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db || !fields || !count) return -1;

    const char *sql = "SELECT id, name, type, options, built_in, "
                      "ref_table, ref_field, formula, sort_order, created_at "
                      "FROM fields_def ORDER BY sort_order ASC, id ASC";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    /* 先统计数量 */
    int n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) n++;
    sqlite3_reset(stmt);

    if (n == 0) {
        *fields = NULL;
        *count = 0;
        sqlite3_finalize(stmt);
        return 0;
    }

    field_def_t *arr = (field_def_t *)calloc(n, sizeof(field_def_t));
    if (!arr) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < n) {
        arr[i].id = sqlite3_column_int64(stmt, 0);
        snprintf(arr[i].name, sizeof(arr[i].name), "%s",
                 (const char *)sqlite3_column_text(stmt, 1));
        arr[i].type = field_type_from_string(
            (const char *)sqlite3_column_text(stmt, 2));
        snprintf(arr[i].options, sizeof(arr[i].options), "%s",
                 (const char *)sqlite3_column_text(stmt, 3));
        arr[i].built_in = sqlite3_column_int(stmt, 4);
        snprintf(arr[i].ref_table, sizeof(arr[i].ref_table), "%s",
                 (const char *)sqlite3_column_text(stmt, 5));
        snprintf(arr[i].ref_field, sizeof(arr[i].ref_field), "%s",
                 (const char *)sqlite3_column_text(stmt, 6));
        snprintf(arr[i].formula, sizeof(arr[i].formula), "%s",
                 (const char *)sqlite3_column_text(stmt, 7));
        arr[i].sort_order = sqlite3_column_int(stmt, 8);
        arr[i].created_at = sqlite3_column_int64(stmt, 9);
        i++;
    }

    sqlite3_finalize(stmt);

    *fields = arr;
    *count = n;
    return 0;
}

void field_def_list_free(field_def_t *fields, int count) {
    (void)count;
    free(fields);
}

int field_def_update_sort(int64_t id, int sort_order) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    const char *sql = "UPDATE fields_def SET sort_order=? WHERE id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, sort_order);
    sqlite3_bind_int64(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ============================================================
 * 字段值 EAV 读写
 * ============================================================ */

int field_value_set(int64_t todo_id, int64_t field_id, const char *value) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    const char *sql = "INSERT OR REPLACE INTO field_values (todo_id, field_id, value) "
                      "VALUES (?, ?, ?)";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, todo_id);
    sqlite3_bind_int64(stmt, 2, field_id);
    sqlite3_bind_text(stmt, 3, value ? value : "", -1, SQLITE_STATIC);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int field_value_get(int64_t todo_id, int64_t field_id, char *value, size_t value_size) {
    sqlite3 *db = todo_db_handle();
    if (!db || !value) return -1;

    const char *sql = "SELECT value FROM field_values WHERE todo_id=? AND field_id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, todo_id);
    sqlite3_bind_int64(stmt, 2, field_id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }

    const char *v = (const char *)sqlite3_column_text(stmt, 0);
    snprintf(value, value_size, "%s", v ? v : "");

    sqlite3_finalize(stmt);
    return 0;
}

int field_value_delete(int64_t todo_id, int64_t field_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    const char *sql = "DELETE FROM field_values WHERE todo_id=? AND field_id=?";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, todo_id);
    sqlite3_bind_int64(stmt, 2, field_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int field_value_list_by_todo(int64_t todo_id, field_value_t **values, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db || !values || !count) return -1;

    const char *sql = "SELECT fv.todo_id, fv.field_id, fv.value "
                      "FROM field_values fv "
                      "WHERE fv.todo_id=? "
                      "ORDER BY fv.field_id ASC";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, todo_id);

    /* 先统计数量 */
    int n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) n++;
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, todo_id);

    if (n == 0) {
        *values = NULL;
        *count = 0;
        sqlite3_finalize(stmt);
        return 0;
    }

    field_value_t *arr = (field_value_t *)calloc(n, sizeof(field_value_t));
    if (!arr) {
        sqlite3_finalize(stmt);
        return -1;
    }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < n) {
        arr[i].todo_id = sqlite3_column_int64(stmt, 0);
        arr[i].field_id = sqlite3_column_int64(stmt, 1);
        snprintf(arr[i].value, sizeof(arr[i].value), "%s",
                 (const char *)sqlite3_column_text(stmt, 2));
        i++;
    }

    sqlite3_finalize(stmt);

    *values = arr;
    *count = n;
    return 0;
}

void field_value_list_free(field_value_t *values, int count) {
    (void)count;
    free(values);
}

int field_value_set_batch(int64_t todo_id, const field_value_t *field_values, int count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    const char *sql = "INSERT OR REPLACE INTO field_values (todo_id, field_id, value) "
                      "VALUES (?, ?, ?)";

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_exec(db, "BEGIN", NULL, NULL, NULL);

    for (int i = 0; i < count; i++) {
        sqlite3_bind_int64(stmt, 1, todo_id);
        sqlite3_bind_int64(stmt, 2, field_values[i].field_id);
        sqlite3_bind_text(stmt, 3, field_values[i].value, -1, SQLITE_STATIC);

        int rc = sqlite3_step(stmt);
        sqlite3_reset(stmt);

        if (rc != SQLITE_DONE) {
            sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
            sqlite3_finalize(stmt);
            return -1;
        }
    }

    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    sqlite3_finalize(stmt);
    return 0;
}