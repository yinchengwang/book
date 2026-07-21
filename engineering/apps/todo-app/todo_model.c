#include "todo_model.h"
#include "todo_db.h"
#include "todo_view.h"
#include "todo_field.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

/* ============================================================
 * 全局状态（保留 extern 声明供日历/统计模块使用）
 * ============================================================ */
todo_t           *g_todos = NULL;
int               g_todo_count = 0;
static char             g_db_path[512] = {0};

/* ============================================================
 * 工具函数
 * ============================================================ */
static int64_t now_ts(void) {
    return (int64_t)time(NULL);
}

/* ============================================================
 * 辅助函数：从 SQLite 行读取 todo_t
 * ============================================================ */
static int row_to_todo(sqlite3_stmt *stmt, todo_t *todo) {
    memset(todo, 0, sizeof(*todo));
    todo->id = sqlite3_column_int64(stmt, 0);
    strncpy(todo->title, (const char *)sqlite3_column_text(stmt, 1), TODO_TITLE_MAX - 1);
    todo->title[TODO_TITLE_MAX - 1] = '\0';
    strncpy(todo->description, (const char *)sqlite3_column_text(stmt, 2), TODO_DESC_MAX - 1);
    todo->description[TODO_DESC_MAX - 1] = '\0';
    strncpy(todo->status, (const char *)sqlite3_column_text(stmt, 3), TODO_STATUS_MAX - 1);
    todo->status[TODO_STATUS_MAX - 1] = '\0';
    const char *labels = (const char *)sqlite3_column_text(stmt, 4);
    strncpy(todo->labels, labels ? labels : "[]", TODO_LABELS_MAX - 1);
    todo->labels[TODO_LABELS_MAX - 1] = '\0';
    todo->priority = sqlite3_column_int(stmt, 5);
    todo->due_date = sqlite3_column_int64(stmt, 6);
    todo->group_id = sqlite3_column_int64(stmt, 7);
    todo->sort_order = sqlite3_column_int(stmt, 8);
    todo->todo_type = sqlite3_column_int(stmt, 9);
    todo->original_date = sqlite3_column_int64(stmt, 10);
    todo->carryover_count = sqlite3_column_int(stmt, 11);
    todo->plan_id = sqlite3_column_int64(stmt, 12);
    todo->plan_item_id = sqlite3_column_int64(stmt, 13);
    todo->completed_at = sqlite3_column_int64(stmt, 14);
    todo->postpone_until = sqlite3_column_int64(stmt, 15);
    todo->task_system_id = sqlite3_column_int64(stmt, 16);
    todo->created_at = sqlite3_column_int64(stmt, 17);
    todo->updated_at = sqlite3_column_int64(stmt, 18);
    return 0;
}

/* ============================================================
 * 辅助函数：todo_t 绑定到 prepared statement
 * bind index: 1-16 (todo 字段), 17=id (for UPDATE WHERE)
 * ============================================================ */
static void bind_todo(sqlite3_stmt *stmt, const todo_t *todo) {
    sqlite3_bind_text(stmt, 1, todo->title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, todo->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, todo->status[0] ? todo->status : "open", -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, todo->labels[0] ? todo->labels : "[]", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 5, todo->priority);
    sqlite3_bind_int64(stmt, 6, todo->due_date);
    sqlite3_bind_int64(stmt, 7, todo->group_id);
    sqlite3_bind_int(stmt, 8, todo->sort_order);
    sqlite3_bind_int(stmt, 9, todo->todo_type);
    sqlite3_bind_int64(stmt, 10, todo->original_date);
    sqlite3_bind_int(stmt, 11, todo->carryover_count);
    sqlite3_bind_int64(stmt, 12, todo->plan_id);
    sqlite3_bind_int64(stmt, 13, todo->plan_item_id);
    sqlite3_bind_int64(stmt, 14, todo->completed_at);
    sqlite3_bind_int64(stmt, 15, todo->postpone_until);
    sqlite3_bind_int64(stmt, 16, todo->task_system_id);
}

/* ============================================================
 * find_todo_idx — 保留供日历/统计模块通过 extern 使用
 * 但内部实现改为从 SQLite 查询
 * ============================================================ */
int find_todo_idx(int64_t id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id FROM todos WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_ROW) ? 0 : -1;
}

/* ============================================================
 * 数据库生命周期
 * ============================================================ */
int todo_db_load(const char *db_path) {
    if (!db_path) return -1;
    strncpy(g_db_path, db_path, sizeof(g_db_path) - 1);
    g_db_path[sizeof(g_db_path) - 1] = '\0';

    /* 构造 .db 路径：将 .json 替换为 .db */
    char db_path_ext[512];
    const char *dot = strrchr(db_path, '.');
    if (dot && strcmp(dot, ".json") == 0) {
        size_t base_len = dot - db_path;
        snprintf(db_path_ext, sizeof(db_path_ext), "%.*s.db", (int)base_len, db_path);
    } else {
        snprintf(db_path_ext, sizeof(db_path_ext), "%s.db", db_path);
    }

    /* 打开 SQLite 数据库 */
    if (todo_db_open(db_path_ext) != 0) {
        fprintf(stderr, "SQLite 数据库打开失败: %s\n", db_path_ext);
        return -1;
    }

    /* 检查 JSON 文件是否存在，如果存在则迁移 */
    FILE *fp = fopen(db_path, "rb");
    if (fp) {
        fclose(fp);
        printf("发现 JSON 文件 %s，开始迁移到 SQLite...\n", db_path);
        todo_db_migrate_from_json(db_path);
    }

    return 0;
}

int todo_db_save(void) {
    /* SQLite 模式下不需要显式保存，每次写操作已提交 */
    return 0;
}

void todo_db_shutdown(void) {
    todo_db_close();
    g_todos = NULL;
    g_todo_count = 0;
}

void todo_db_reset(void) {
    todo_db_close();
    g_todos = NULL;
    g_todo_count = 0;
    g_db_path[0] = '\0';
    /* 删除 .db 文件 */
    char db_path_ext[512];
    const char *dot = strrchr(g_db_path, '.');
    if (dot) {
        size_t base_len = dot - g_db_path;
        snprintf(db_path_ext, sizeof(db_path_ext), "%.*s.db", (int)base_len, g_db_path);
    } else {
        snprintf(db_path_ext, sizeof(db_path_ext), "%s.db", g_db_path);
    }
    remove(db_path_ext);
}

/* ============================================================
 * 标签匹配（保留，用于 todo_list 的 labels 过滤）
 * ============================================================ */
static int match_labels(const char *todo_labels, const char *query_labels) {
    if (!query_labels || !query_labels[0]) return 1;
    if (!todo_labels || !todo_labels[0]) return 0;

    char qcpy[TODO_LABELS_MAX];
    strncpy(qcpy, query_labels, sizeof(qcpy) - 1);
    qcpy[sizeof(qcpy) - 1] = '\0';

    char *save = NULL;
    char *tok = strtok_r(qcpy, ",", &save);
    while (tok) {
        while (*tok == ' ') tok++;
        size_t tlen = strlen(tok);
        while (tlen > 0 && (tok[tlen-1] == ' ' || tok[tlen-1] == '"')) tlen--;
        tok[tlen] = '\0';
        if (tok[0] && strstr(todo_labels, tok) == NULL) return 0;
        tok = strtok_r(NULL, ",", &save);
    }
    return 1;
}

/* ============================================================
 * Todo CRUD
 * ============================================================ */
int todo_create(const todo_t *todo, int64_t *out_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT INTO todos (title, description, status, labels, priority, "
        "due_date, group_id, sort_order, todo_type, original_date, carryover_count, "
        "plan_id, plan_item_id, completed_at, postpone_until, task_system_id, "
        "created_at, updated_at) "
        "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18)";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "todo_create prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    bind_todo(stmt, todo);
    int64_t now = now_ts();
    sqlite3_bind_int64(stmt, 17, now);
    sqlite3_bind_int64(stmt, 18, now);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "todo_create step error: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    if (out_id) *out_id = id;
    return 0;
}

int todo_get_by_id(int64_t id, todo_t *todo) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT * FROM todos WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }
    if (todo) row_to_todo(stmt, todo);
    sqlite3_finalize(stmt);
    return 0;
}

int todo_update(const todo_t *todo) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    const char *sql = "UPDATE todos SET title=?1, description=?2, status=?3, labels=?4, "
        "priority=?5, due_date=?6, group_id=?7, sort_order=?8, todo_type=?9, "
        "original_date=?10, carryover_count=?11, plan_id=?12, plan_item_id=?13, "
        "completed_at=?14, postpone_until=?15, task_system_id=?16, "
        "updated_at=CAST(strftime('%s','now') AS INTEGER) "
        "WHERE id=?17";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "todo_update prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    bind_todo(stmt, todo);
    sqlite3_bind_int64(stmt, 17, todo->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "todo_update error: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    return 0;
}

int todo_delete(int64_t id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM todos WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ============================================================
 * 筛选/排序辅助函数（无需绑定参数，直接构建 SQL 片段）
 * 使用安全转义，避免 SQL 注入
 * ============================================================ */
static void escape_sql_string(const char *in, char *out, size_t out_size) {
    size_t j = 0;
    for (size_t i = 0; in[i] && j < out_size - 2; i++) {
        if (in[i] == '\'') {
            if (j + 2 < out_size) { out[j++] = '\''; out[j++] = '\''; }
        } else {
            out[j++] = in[i];
        }
    }
    out[j] = '\0';
}

static void append_filter_sql_inline(const filter_rule_t *rule, char *where, size_t where_size) {
    char buf[1024];
    char escaped[FILTER_VALUE_MAX * 2 + 1];

    if (rule->field_id <= 9) {
        const char *col = NULL;
        switch (rule->field_id) {
            case 1: col = "title"; break;
            case 2: col = "description"; break;
            case 3: col = "status"; break;
            case 4: col = "priority"; break;
            case 5: col = "due_date"; break;
            case 6: col = "labels"; break;
            case 7: col = "group_id"; break;
            default: return;
        }

        if (strcmp(rule->operator, "eq") == 0) {
            escape_sql_string(rule->value, escaped, sizeof(escaped));
            snprintf(buf, sizeof(buf), " AND %s = '%s'", col, escaped);
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "ne") == 0) {
            escape_sql_string(rule->value, escaped, sizeof(escaped));
            snprintf(buf, sizeof(buf), " AND %s != '%s'", col, escaped);
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "gt") == 0) {
            snprintf(buf, sizeof(buf), " AND %s > %lld", col, (long long)atoll(rule->value));
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "lt") == 0) {
            snprintf(buf, sizeof(buf), " AND %s < %lld", col, (long long)atoll(rule->value));
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "contains") == 0) {
            escape_sql_string(rule->value, escaped, sizeof(escaped));
            snprintf(buf, sizeof(buf), " AND %s LIKE '%%%s%%'", col, escaped);
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "gte") == 0) {
            snprintf(buf, sizeof(buf), " AND %s >= %lld", col, (long long)atoll(rule->value));
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "lte") == 0) {
            snprintf(buf, sizeof(buf), " AND %s <= %lld", col, (long long)atoll(rule->value));
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "is_empty") == 0) {
            snprintf(buf, sizeof(buf), " AND (%s = '' OR %s IS NULL)", col, col);
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "is_not_empty") == 0) {
            snprintf(buf, sizeof(buf), " AND (%s != '' AND %s IS NOT NULL)", col, col);
            strncat(where, buf, where_size - strlen(where) - 1);
        }
    } else {
        if (strcmp(rule->operator, "eq") == 0) {
            escape_sql_string(rule->value, escaped, sizeof(escaped));
            snprintf(buf, sizeof(buf),
                " AND EXISTS (SELECT 1 FROM field_values fv%lld WHERE fv%lld.todo_id = todos.id AND fv%lld.field_id = %lld AND fv%lld.value = '%s')",
                (long long)rule->field_id, (long long)rule->field_id,
                (long long)rule->field_id, (long long)rule->field_id,
                escaped);
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "contains") == 0) {
            escape_sql_string(rule->value, escaped, sizeof(escaped));
            snprintf(buf, sizeof(buf),
                " AND EXISTS (SELECT 1 FROM field_values fv%lld WHERE fv%lld.todo_id = todos.id AND fv%lld.field_id = %lld AND fv%lld.value LIKE '%%%s%%')",
                (long long)rule->field_id, (long long)rule->field_id,
                (long long)rule->field_id, (long long)rule->field_id,
                escaped);
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "is_empty") == 0) {
            snprintf(buf, sizeof(buf),
                " AND NOT EXISTS (SELECT 1 FROM field_values fv%lld WHERE fv%lld.todo_id = todos.id AND fv%lld.field_id = %lld)",
                (long long)rule->field_id, (long long)rule->field_id,
                (long long)rule->field_id);
            strncat(where, buf, where_size - strlen(where) - 1);
        } else if (strcmp(rule->operator, "is_not_empty") == 0) {
            snprintf(buf, sizeof(buf),
                " AND EXISTS (SELECT 1 FROM field_values fv%lld WHERE fv%lld.todo_id = todos.id AND fv%lld.field_id = %lld AND fv%lld.value != '')",
                (long long)rule->field_id, (long long)rule->field_id,
                (long long)rule->field_id,
                (long long)rule->field_id);
            strncat(where, buf, where_size - strlen(where) - 1);
        }
    }
}

static void append_sort_sql_inline(const sort_rule_t *rules, int count, char *order_by, size_t order_by_size) {
    for (int i = 0; i < count; i++) {
        const char *dir = (strcmp(rules[i].direction, "desc") == 0) ? "DESC" : "ASC";
        char buf[128];

        if (rules[i].field_id <= 9) {
            const char *col = NULL;
            switch (rules[i].field_id) {
                case 1: col = "title"; break;
                case 2: col = "description"; break;
                case 3: col = "status"; break;
                case 4: col = "priority"; break;
                case 5: col = "due_date"; break;
                case 6: col = "labels"; break;
                case 7: col = "group_id"; break;
                case 8: col = "created_at"; break;
                case 9: col = "updated_at"; break;
                default: continue;
            }
            snprintf(buf, sizeof(buf), "%s %s %s", i > 0 ? "," : " ORDER BY", col, dir);
        } else {
            /* 扩展字段排序 — 支持数值类型按 CAST AS REAL 排序 */
            field_def_t fdef;
            if (field_def_get(rules[i].field_id, &fdef) == 0) {
                if (fdef.type == FIELD_TYPE_NUMBER) {
                    snprintf(buf, sizeof(buf), "%s (SELECT CAST(value AS REAL) FROM field_values fv WHERE fv.todo_id = todos.id AND fv.field_id = %lld) %s",
                             i > 0 ? "," : " ORDER BY",
                             (long long)rules[i].field_id, dir);
                } else {
                    snprintf(buf, sizeof(buf), "%s (SELECT value FROM field_values fv WHERE fv.todo_id = todos.id AND fv.field_id = %lld) %s",
                             i > 0 ? "," : " ORDER BY",
                             (long long)rules[i].field_id, dir);
                }
                strncat(order_by, buf, order_by_size - strlen(order_by) - 1);
            }
            continue;
        }
        strncat(order_by, buf, order_by_size - strlen(order_by) - 1);
    }
}

static void parse_view_config_filters(const char *config_json,
                                       filter_rule_t *filters, int *count) {
    *count = 0;
    cJSON *jconfig = cJSON_Parse(config_json);
    if (!jconfig) return;

    cJSON *jfilters = cJSON_GetObjectItem(jconfig, "filters");
    if (jfilters && cJSON_IsArray(jfilters)) {
        int n = cJSON_GetArraySize(jfilters);
        if (n > MAX_FILTERS) n = MAX_FILTERS;
        for (int i = 0; i < n; i++) {
            cJSON *jrule = cJSON_GetArrayItem(jfilters, i);
            if (!jrule) continue;
            cJSON *jfid = cJSON_GetObjectItem(jrule, "field_id");
            cJSON *jop = cJSON_GetObjectItem(jrule, "operator");
            cJSON *jval = cJSON_GetObjectItem(jrule, "value");
            if (!jfid || !jop || !jval) continue;

            filters[*count].field_id = (int64_t)cJSON_GetNumberValue(jfid);
            const char *op_str = cJSON_GetStringValue(jop);
            if (op_str) snprintf(filters[*count].operator, sizeof(filters[*count].operator), "%s", op_str);
            const char *val_str = cJSON_GetStringValue(jval);
            if (val_str) snprintf(filters[*count].value, sizeof(filters[*count].value), "%s", val_str);
            (*count)++;
        }
    }

    cJSON_Delete(jconfig);
}

static void parse_view_config_sorts(const char *config_json,
                                    sort_rule_t *sorts, int *count) {
    *count = 0;
    cJSON *jconfig = cJSON_Parse(config_json);
    if (!jconfig) return;

    cJSON *jsort = cJSON_GetObjectItem(jconfig, "sort");
    if (jsort && cJSON_IsArray(jsort)) {
        int n = cJSON_GetArraySize(jsort);
        if (n > MAX_SORTS) n = MAX_SORTS;
        for (int i = 0; i < n; i++) {
            cJSON *jrule = cJSON_GetArrayItem(jsort, i);
            if (!jrule) continue;
            cJSON *jfid = cJSON_GetObjectItem(jrule, "field_id");
            cJSON *jdir = cJSON_GetObjectItem(jrule, "direction");
            if (!jfid || !jdir) continue;

            sorts[*count].field_id = (int64_t)cJSON_GetNumberValue(jfid);
            const char *dir_str = cJSON_GetStringValue(jdir);
            if (dir_str) snprintf(sorts[*count].direction, sizeof(sorts[*count].direction), "%s", dir_str);
            (*count)++;
        }
    }

    cJSON_Delete(jconfig);
}

int todo_list(const todo_query_t *query, todo_list_t *result) {
    memset(result, 0, sizeof(*result));
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    /* 构建动态 SQL */
    char sql_base[4096];
    char where[4096] = "WHERE 1=1";
    int param_idx = 1;
    /* 用于存储绑定值，避免临时变量生命周期问题 */
    char status_buf[64] = {0};
    char search_buf[512] = {0};
    char labels_buf[1024] = {0};

    /* 如果指定了 view_id，从视图配置加载 filter/sort */
    filter_rule_t filters[MAX_FILTERS];
    sort_rule_t sorts[MAX_SORTS];
    int filter_count = 0, sort_count = 0;

    if (query->view_id > 0) {
        view_t view;
        if (view_get(query->view_id, &view) == 0 && view.config[0]) {
            parse_view_config_filters(view.config, filters, &filter_count);
            parse_view_config_sorts(view.config, sorts, &sort_count);
        }
    } else {
        filter_count = query->filter_count;
        sort_count = query->sort_count;
        if (filter_count > 0)
            memcpy(filters, query->filter_rules, sizeof(filter_rule_t) * (filter_count > MAX_FILTERS ? MAX_FILTERS : filter_count));
        if (sort_count > 0)
            memcpy(sorts, query->sort_rules, sizeof(sort_rule_t) * (sort_count > MAX_SORTS ? MAX_SORTS : sort_count));
    }

    if (query->status && strcmp(query->status, "all") != 0 && query->status[0]) {
        strncpy(status_buf, query->status, sizeof(status_buf) - 1);
        char cond[256];
        snprintf(cond, sizeof(cond), " AND status=?%d", param_idx++);
        strcat(where, cond);
    }
    if (query->labels && query->labels[0]) {
        strncpy(labels_buf, query->labels, sizeof(labels_buf) - 1);
        char cond[256];
        snprintf(cond, sizeof(cond), " AND labels LIKE '%%' || ?%d || '%%'", param_idx++);
        strcat(where, cond);
    }
    if (query->search && query->search[0]) {
        strncpy(search_buf, query->search, sizeof(search_buf) - 1);
        char cond[512];
        snprintf(cond, sizeof(cond), " AND (title LIKE '%%' || ?%d || '%%' OR description LIKE '%%' || ?%d || '%%')", param_idx, param_idx);
        param_idx++;
        strcat(where, cond);
    }
    if (query->priority >= 0) {
        char cond[128];
        snprintf(cond, sizeof(cond), " AND priority=?%d", param_idx++);
        strcat(where, cond);
    }
    if (query->group_id >= 0) {
        char cond[128];
        snprintf(cond, sizeof(cond), " AND group_id=?%d", param_idx++);
        strcat(where, cond);
    }
    if (query->due_before > 0) {
        char cond[128];
        snprintf(cond, sizeof(cond), " AND (due_date=0 OR due_date<=?%d)", param_idx++);
        strcat(where, cond);
    }
    if (query->due_after > 0) {
        char cond[128];
        snprintf(cond, sizeof(cond), " AND (due_date=0 OR due_date>=?%d)", param_idx++);
        strcat(where, cond);
    }

    /* 应用筛选规则（从视图配置加载的） */
    for (int i = 0; i < filter_count; i++) {
        append_filter_sql_inline(&filters[i], where, sizeof(where));
    }

    /* 排序 */
    char order_clause[1024] = "";
    if (sort_count > 0) {
        /* 使用视图配置的排序规则 */
        append_sort_sql_inline(sorts, sort_count, order_clause, sizeof(order_clause));
    } else {
        /* 使用 query 中的单字段排序 */
        const char *order_field = "sort_order";
        if (query->sort) {
            if (strcmp(query->sort, "priority") == 0) order_field = "priority";
            else if (strcmp(query->sort, "due_date") == 0) order_field = "due_date";
            else if (strcmp(query->sort, "created_at") == 0) order_field = "created_at";
            else if (strcmp(query->sort, "sort_order") == 0) order_field = "sort_order";
        }
        const char *order_dir = query->sort_desc ? "DESC" : "ASC";
        snprintf(order_clause, sizeof(order_clause), " ORDER BY %s %s", order_field, order_dir);
    }

    /* 先查询总数 */
    snprintf(sql_base, sizeof(sql_base), "SELECT COUNT(*) FROM todos %s", where);
    sqlite3_stmt *count_stmt = NULL;
    if (sqlite3_prepare_v2(db, sql_base, -1, &count_stmt, NULL) != SQLITE_OK) return -1;
    int p = 1;
    if (query->status && strcmp(query->status, "all") != 0 && query->status[0])
        sqlite3_bind_text(count_stmt, p++, status_buf, -1, SQLITE_STATIC);
    if (query->labels && query->labels[0])
        sqlite3_bind_text(count_stmt, p++, labels_buf, -1, SQLITE_STATIC);
    if (query->search && query->search[0])
        sqlite3_bind_text(count_stmt, p++, search_buf, -1, SQLITE_STATIC);
    if (query->priority >= 0)
        sqlite3_bind_int(count_stmt, p++, query->priority);
    if (query->group_id >= 0)
        sqlite3_bind_int64(count_stmt, p++, query->group_id);
    if (query->due_before > 0)
        sqlite3_bind_int64(count_stmt, p++, query->due_before);
    if (query->due_after > 0)
        sqlite3_bind_int64(count_stmt, p++, query->due_after);

    int total = 0;
    if (sqlite3_step(count_stmt) == SQLITE_ROW) {
        total = sqlite3_column_int(count_stmt, 0);
    }
    sqlite3_finalize(count_stmt);

    /* 分页参数 */
    int page = query->page > 0 ? query->page : 1;
    int per_page = query->per_page > 0 ? query->per_page : 20;
    if (per_page > 100) per_page = 100;
    int offset = (page - 1) * per_page;

    /* 查询数据 */
    snprintf(sql_base, sizeof(sql_base),
        "SELECT * FROM todos %s %s LIMIT ?%d OFFSET ?%d",
        where, order_clause, param_idx, param_idx + 1);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql_base, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "todo_list prepare error: %s\n", sqlite3_errmsg(db));
        return -1;
    }

    p = 1;
    if (query->status && strcmp(query->status, "all") != 0 && query->status[0])
        sqlite3_bind_text(stmt, p++, status_buf, -1, SQLITE_STATIC);
    if (query->labels && query->labels[0])
        sqlite3_bind_text(stmt, p++, labels_buf, -1, SQLITE_STATIC);
    if (query->search && query->search[0])
        sqlite3_bind_text(stmt, p++, search_buf, -1, SQLITE_STATIC);
    if (query->priority >= 0)
        sqlite3_bind_int(stmt, p++, query->priority);
    if (query->group_id >= 0)
        sqlite3_bind_int64(stmt, p++, query->group_id);
    if (query->due_before > 0)
        sqlite3_bind_int64(stmt, p++, query->due_before);
    if (query->due_after > 0)
        sqlite3_bind_int64(stmt, p++, query->due_after);
    sqlite3_bind_int(stmt, p++, per_page);
    sqlite3_bind_int(stmt, p++, offset);

    /* 收集结果 */
    int count = 0, cap = 16;
    todo_t *items = malloc(cap * sizeof(todo_t));
    if (!items) { sqlite3_finalize(stmt); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (count >= cap) {
            cap *= 2;
            todo_t *new_items = realloc(items, cap * sizeof(todo_t));
            if (!new_items) { free(items); sqlite3_finalize(stmt); return -1; }
            items = new_items;
        }
        row_to_todo(stmt, &items[count++]);
    }
    sqlite3_finalize(stmt);

    result->items = items;
    result->count = count;
    result->total = total;
    return 0;
}

void todo_list_free(todo_list_t *result) {
    if (!result) return;
    free(result->items);
    result->items = NULL;
    result->count = 0;
    result->total = 0;
}

int todo_update_sort(int64_t id, int sort_order) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE todos SET sort_order=?, updated_at=CAST(strftime('%s','now') AS INTEGER) WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, sort_order);
    sqlite3_bind_int64(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int todo_get_stats(todo_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    int64_t now = now_ts();
    int64_t today_start = (now / 86400) * 86400;
    int64_t today_end = today_start + 86400;

    /* 总计数 */
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v2(db, "SELECT COUNT(*), SUM(status='open'), SUM(status='closed'), SUM(status='archived') FROM todos", -1, &stmt, NULL);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        stats->total = sqlite3_column_int(stmt, 0);
        stats->open = sqlite3_column_int(stmt, 1);
        stats->closed = sqlite3_column_int(stmt, 2);
        stats->archived = sqlite3_column_int(stmt, 3);
    }
    sqlite3_finalize(stmt);

    /* 过期数 */
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM todos WHERE due_date>0 AND due_date<? AND status='open'", -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, now);
    if (sqlite3_step(stmt) == SQLITE_ROW) stats->overdue = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    /* 今日到期 */
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM todos WHERE due_date>=? AND due_date<? AND status='open'", -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, today_start);
    sqlite3_bind_int64(stmt, 2, today_end);
    if (sqlite3_step(stmt) == SQLITE_ROW) stats->due_today = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);

    int active = stats->open + stats->closed;
    stats->completion_rate = active > 0 ? (double)stats->closed / active : 0.0;
    return 0;
}

/* ============================================================
 * Checklist
 * ============================================================ */
int checklist_add(int64_t todo_id, const char *text, checklist_item_t *item) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    /* 自动分配 sort_order */
    if (sqlite3_prepare_v2(db,
        "INSERT INTO checklist (todo_id, text, done, sort_order) VALUES (?,?,0,"
        "(SELECT COALESCE(MAX(sort_order),0)+1 FROM checklist WHERE todo_id=?))",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, todo_id);
    sqlite3_bind_text(stmt, 2, text, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, todo_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    if (item) {
        /* 回读取创建的数据 */
        sqlite3_prepare_v2(db, "SELECT id, todo_id, text, done, sort_order FROM checklist WHERE id=?", -1, &stmt, NULL);
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            memset(item, 0, sizeof(*item));
            item->id = sqlite3_column_int64(stmt, 0);
            item->todo_id = sqlite3_column_int64(stmt, 1);
            strncpy(item->text, (const char *)sqlite3_column_text(stmt, 2), CHECKLIST_TEXT_MAX - 1);
            item->done = sqlite3_column_int(stmt, 3);
            item->sort_order = sqlite3_column_int(stmt, 4);
        }
        sqlite3_finalize(stmt);
    }
    return 0;
}

int checklist_toggle(int64_t item_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE checklist SET done=1-done WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, item_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int checklist_delete(int64_t item_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM checklist WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, item_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int checklist_list(int64_t todo_id, checklist_item_t **items, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, todo_id, text, done, sort_order FROM checklist WHERE todo_id=? ORDER BY sort_order",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, todo_id);

    int cap = 8, cnt = 0;
    checklist_item_t *list = malloc(cap * sizeof(checklist_item_t));
    if (!list) { sqlite3_finalize(stmt); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cnt >= cap) {
            cap *= 2;
            checklist_item_t *new_list = realloc(list, cap * sizeof(checklist_item_t));
            if (!new_list) { free(list); sqlite3_finalize(stmt); return -1; }
            list = new_list;
        }
        memset(&list[cnt], 0, sizeof(checklist_item_t));
        list[cnt].id = sqlite3_column_int64(stmt, 0);
        list[cnt].todo_id = sqlite3_column_int64(stmt, 1);
        strncpy(list[cnt].text, (const char *)sqlite3_column_text(stmt, 2), CHECKLIST_TEXT_MAX - 1);
        list[cnt].done = sqlite3_column_int(stmt, 3);
        list[cnt].sort_order = sqlite3_column_int(stmt, 4);
        cnt++;
    }
    sqlite3_finalize(stmt);

    *items = list;
    *count = cnt;
    return 0;
}

void checklist_list_free(checklist_item_t *items, int count) {
    (void)count;
    if (items) free(items);
}

/* ============================================================
 * 分组 CRUD（表名：groups_t）
 * ============================================================ */
int group_create(const group_t *group, int64_t *out_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO groups_t (name, color, sort_order, created_at) VALUES (?,?,?,CAST(strftime('%s','now') AS INTEGER))",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, group->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, group->color[0] ? group->color : "#4A90D9", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, group->sort_order);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    if (out_id) *out_id = id;
    return 0;
}

int group_get(int64_t id, group_t *group) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, color, sort_order, created_at FROM groups_t WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return -1; }
    if (group) {
        memset(group, 0, sizeof(*group));
        group->id = sqlite3_column_int64(stmt, 0);
        strncpy(group->name, (const char *)sqlite3_column_text(stmt, 1), GROUP_NAME_MAX - 1);
        strncpy(group->color, (const char *)sqlite3_column_text(stmt, 2), GROUP_COLOR_MAX - 1);
        group->sort_order = sqlite3_column_int(stmt, 3);
        group->created_at = sqlite3_column_int64(stmt, 4);
    }
    sqlite3_finalize(stmt);
    return 0;
}

int group_update(const group_t *group) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE groups_t SET name=?, color=?, sort_order=? WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, group->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, group->color, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 3, group->sort_order);
    sqlite3_bind_int64(stmt, 4, group->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int group_delete(int64_t id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    /* 将该分组的待办置为未分组 */
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v2(db, "UPDATE todos SET group_id=0 WHERE group_id=?", -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    /* 删除分组 */
    sqlite3_prepare_v2(db, "DELETE FROM groups_t WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int group_list(group_t **groups, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, color, sort_order, created_at FROM groups_t ORDER BY sort_order",
        -1, &stmt, NULL) != SQLITE_OK) return -1;

    int cap = 8, cnt = 0;
    group_t *list = malloc(cap * sizeof(group_t));
    if (!list) { sqlite3_finalize(stmt); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cnt >= cap) {
            cap *= 2;
            group_t *new_list = realloc(list, cap * sizeof(group_t));
            if (!new_list) { free(list); sqlite3_finalize(stmt); return -1; }
            list = new_list;
        }
        memset(&list[cnt], 0, sizeof(group_t));
        list[cnt].id = sqlite3_column_int64(stmt, 0);
        strncpy(list[cnt].name, (const char *)sqlite3_column_text(stmt, 1), GROUP_NAME_MAX - 1);
        strncpy(list[cnt].color, (const char *)sqlite3_column_text(stmt, 2), GROUP_COLOR_MAX - 1);
        list[cnt].sort_order = sqlite3_column_int(stmt, 3);
        list[cnt].created_at = sqlite3_column_int64(stmt, 4);
        cnt++;
    }
    sqlite3_finalize(stmt);

    *groups = list;
    *count = cnt;
    return 0;
}

void group_list_free(group_t *groups, int count) {
    (void)count;
    if (groups) free(groups);
}

/* ============================================================
 * 评论 CRUD
 * ============================================================ */
int comment_add(int64_t todo_id, const char *text, comment_t *comment) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO comments (todo_id, text, created_at) VALUES (?,?,CAST(strftime('%s','now') AS INTEGER))",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, todo_id);
    sqlite3_bind_text(stmt, 2, text, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);

    if (comment) {
        sqlite3_prepare_v2(db, "SELECT id, todo_id, text, created_at FROM comments WHERE id=?", -1, &stmt, NULL);
        sqlite3_bind_int64(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            memset(comment, 0, sizeof(*comment));
            comment->id = sqlite3_column_int64(stmt, 0);
            comment->todo_id = sqlite3_column_int64(stmt, 1);
            strncpy(comment->text, (const char *)sqlite3_column_text(stmt, 2), COMMENT_TEXT_MAX - 1);
            comment->created_at = sqlite3_column_int64(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }
    return 0;
}

int comment_list(int64_t todo_id, comment_t **comments, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, todo_id, text, created_at FROM comments WHERE todo_id=? ORDER BY created_at",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, todo_id);

    int cap = 8, cnt = 0;
    comment_t *list = malloc(cap * sizeof(comment_t));
    if (!list) { sqlite3_finalize(stmt); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cnt >= cap) {
            cap *= 2;
            comment_t *new_list = realloc(list, cap * sizeof(comment_t));
            if (!new_list) { free(list); sqlite3_finalize(stmt); return -1; }
            list = new_list;
        }
        memset(&list[cnt], 0, sizeof(comment_t));
        list[cnt].id = sqlite3_column_int64(stmt, 0);
        list[cnt].todo_id = sqlite3_column_int64(stmt, 1);
        strncpy(list[cnt].text, (const char *)sqlite3_column_text(stmt, 2), COMMENT_TEXT_MAX - 1);
        list[cnt].created_at = sqlite3_column_int64(stmt, 3);
        cnt++;
    }
    sqlite3_finalize(stmt);

    *comments = list;
    *count = cnt;
    return 0;
}

void comment_list_free(comment_t *comments, int count) {
    (void)count;
    if (comments) free(comments);
}

int comment_delete(int64_t comment_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM comments WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, comment_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

/* ============================================================
 * 任务系统 CRUD
 * ============================================================ */
int task_system_create(const task_system_t *ts, int64_t *out_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO task_systems (name, description, color, sort_order, created_at) VALUES (?,?,?,?,CAST(strftime('%s','now') AS INTEGER))",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, ts->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ts->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ts->color[0] ? ts->color : "#4A90D9", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, ts->sort_order);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    if (out_id) *out_id = id;
    return 0;
}

int task_system_get(int64_t id, task_system_t *ts) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, description, color, sort_order, created_at FROM task_systems WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return -1; }
    if (ts) {
        memset(ts, 0, sizeof(*ts));
        ts->id = sqlite3_column_int64(stmt, 0);
        strncpy(ts->name, (const char *)sqlite3_column_text(stmt, 1), TASK_SYSTEM_NAME_MAX - 1);
        strncpy(ts->description, (const char *)sqlite3_column_text(stmt, 2), 1023);
        strncpy(ts->color, (const char *)sqlite3_column_text(stmt, 3), 15);
        ts->sort_order = sqlite3_column_int(stmt, 4);
        ts->created_at = sqlite3_column_int64(stmt, 5);
    }
    sqlite3_finalize(stmt);
    return 0;
}

int task_system_update(const task_system_t *ts) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "UPDATE task_systems SET name=?, description=?, color=?, sort_order=? WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, ts->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, ts->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, ts->color, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, ts->sort_order);
    sqlite3_bind_int64(stmt, 5, ts->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int task_system_delete(int64_t id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    /* 将该系统的待办置为默认系统 */
    sqlite3_stmt *stmt = NULL;
    sqlite3_prepare_v2(db, "UPDATE todos SET task_system_id=1 WHERE task_system_id=?", -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    sqlite3_prepare_v2(db, "DELETE FROM task_systems WHERE id=?", -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int task_system_list(task_system_t **systems, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, description, color, sort_order, created_at FROM task_systems ORDER BY sort_order",
        -1, &stmt, NULL) != SQLITE_OK) return -1;

    int cap = 8, cnt = 0;
    task_system_t *list = malloc(cap * sizeof(task_system_t));
    if (!list) { sqlite3_finalize(stmt); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cnt >= cap) {
            cap *= 2;
            task_system_t *new_list = realloc(list, cap * sizeof(task_system_t));
            if (!new_list) { free(list); sqlite3_finalize(stmt); return -1; }
            list = new_list;
        }
        memset(&list[cnt], 0, sizeof(task_system_t));
        list[cnt].id = sqlite3_column_int64(stmt, 0);
        strncpy(list[cnt].name, (const char *)sqlite3_column_text(stmt, 1), TASK_SYSTEM_NAME_MAX - 1);
        strncpy(list[cnt].description, (const char *)sqlite3_column_text(stmt, 2), 1023);
        strncpy(list[cnt].color, (const char *)sqlite3_column_text(stmt, 3), 15);
        list[cnt].sort_order = sqlite3_column_int(stmt, 4);
        list[cnt].created_at = sqlite3_column_int64(stmt, 5);
        cnt++;
    }
    sqlite3_finalize(stmt);

    *systems = list;
    *count = cnt;
    return 0;
}

void task_system_list_free(task_system_t *systems, int count) {
    (void)count;
    if (systems) free(systems);
}

/* ============================================================
 * 学习计划 CRUD
 * ============================================================ */
int plan_create(const plan_t *plan, int64_t *out_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO plans (name, description, start_date, end_date, color, status, created_at, updated_at) "
        "VALUES (?,?,?,?,?,?,CAST(strftime('%s','now') AS INTEGER),CAST(strftime('%s','now') AS INTEGER))",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, plan->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, plan->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, plan->start_date);
    sqlite3_bind_int64(stmt, 4, plan->end_date);
    sqlite3_bind_text(stmt, 5, plan->color[0] ? plan->color : "#4A90D9", -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, plan->status);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    if (out_id) *out_id = id;
    return 0;
}

int plan_get(int64_t id, plan_t *plan) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, description, start_date, end_date, color, status, created_at, updated_at FROM plans WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return -1; }
    if (plan) {
        memset(plan, 0, sizeof(*plan));
        plan->id = sqlite3_column_int64(stmt, 0);
        strncpy(plan->name, (const char *)sqlite3_column_text(stmt, 1), PLAN_NAME_MAX - 1);
        strncpy(plan->description, (const char *)sqlite3_column_text(stmt, 2), PLAN_DESC_MAX - 1);
        plan->start_date = sqlite3_column_int64(stmt, 3);
        plan->end_date = sqlite3_column_int64(stmt, 4);
        strncpy(plan->color, (const char *)sqlite3_column_text(stmt, 5), 15);
        plan->status = sqlite3_column_int(stmt, 6);
        plan->created_at = sqlite3_column_int64(stmt, 7);
        plan->updated_at = sqlite3_column_int64(stmt, 8);
    }
    sqlite3_finalize(stmt);
    return 0;
}

int plan_update(const plan_t *plan) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "UPDATE plans SET name=?, description=?, start_date=?, end_date=?, color=?, status=?, "
        "updated_at=CAST(strftime('%s','now') AS INTEGER) WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, plan->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, plan->description, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, plan->start_date);
    sqlite3_bind_int64(stmt, 4, plan->end_date);
    sqlite3_bind_text(stmt, 5, plan->color, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 6, plan->status);
    sqlite3_bind_int64(stmt, 7, plan->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int plan_delete(int64_t id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM plans WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    /* 外键级联删除 plan_items */
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int plan_list(plan_t **plans, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT id, name, description, start_date, end_date, color, status, created_at, updated_at FROM plans ORDER BY id",
        -1, &stmt, NULL) != SQLITE_OK) return -1;

    int cap = 8, cnt = 0;
    plan_t *list = malloc(cap * sizeof(plan_t));
    if (!list) { sqlite3_finalize(stmt); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cnt >= cap) {
            cap *= 2;
            plan_t *new_list = realloc(list, cap * sizeof(plan_t));
            if (!new_list) { free(list); sqlite3_finalize(stmt); return -1; }
            list = new_list;
        }
        memset(&list[cnt], 0, sizeof(plan_t));
        list[cnt].id = sqlite3_column_int64(stmt, 0);
        strncpy(list[cnt].name, (const char *)sqlite3_column_text(stmt, 1), PLAN_NAME_MAX - 1);
        strncpy(list[cnt].description, (const char *)sqlite3_column_text(stmt, 2), PLAN_DESC_MAX - 1);
        list[cnt].start_date = sqlite3_column_int64(stmt, 3);
        list[cnt].end_date = sqlite3_column_int64(stmt, 4);
        strncpy(list[cnt].color, (const char *)sqlite3_column_text(stmt, 5), 15);
        list[cnt].status = sqlite3_column_int(stmt, 6);
        list[cnt].created_at = sqlite3_column_int64(stmt, 7);
        list[cnt].updated_at = sqlite3_column_int64(stmt, 8);
        cnt++;
    }
    sqlite3_finalize(stmt);

    *plans = list;
    *count = cnt;
    return 0;
}

void plan_list_free(plan_t *plans, int count) {
    (void)count;
    if (plans) free(plans);
}

/* ============================================================
 * 计划项 CRUD
 * ============================================================ */
int plan_item_create(const plan_item_t *item, int64_t *out_id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "INSERT INTO plan_items (plan_id, parent_id, title, item_type, planned_date, "
        "estimated_minutes, order_index, completion_rule, todo_id, actual_minutes) "
        "VALUES (?,?,?,?,?,?,?,?,?,?)",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, item->plan_id);
    sqlite3_bind_int64(stmt, 2, item->parent_id);
    sqlite3_bind_text(stmt, 3, item->title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, item->item_type);
    sqlite3_bind_int64(stmt, 5, item->planned_date);
    sqlite3_bind_int(stmt, 6, item->estimated_minutes);
    sqlite3_bind_int(stmt, 7, item->order_index);
    sqlite3_bind_int(stmt, 8, item->completion_rule);
    sqlite3_bind_int64(stmt, 9, item->todo_id);
    sqlite3_bind_int(stmt, 10, item->actual_minutes);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        return -1;
    }
    int64_t id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(stmt);
    if (out_id) *out_id = id;
    return 0;
}

int plan_item_get(int64_t id, plan_item_t *item) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT id, plan_id, parent_id, title, item_type, planned_date, "
        "estimated_minutes, order_index, completion_rule, todo_id, actual_minutes FROM plan_items WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);

    int rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) { sqlite3_finalize(stmt); return -1; }
    if (item) {
        memset(item, 0, sizeof(*item));
        item->id = sqlite3_column_int64(stmt, 0);
        item->plan_id = sqlite3_column_int64(stmt, 1);
        item->parent_id = sqlite3_column_int64(stmt, 2);
        strncpy(item->title, (const char *)sqlite3_column_text(stmt, 3), ITEM_TITLE_MAX - 1);
        item->item_type = sqlite3_column_int(stmt, 4);
        item->planned_date = sqlite3_column_int64(stmt, 5);
        item->estimated_minutes = sqlite3_column_int(stmt, 6);
        item->order_index = sqlite3_column_int(stmt, 7);
        item->completion_rule = sqlite3_column_int(stmt, 8);
        item->todo_id = sqlite3_column_int64(stmt, 9);
        item->actual_minutes = sqlite3_column_int(stmt, 10);
    }
    sqlite3_finalize(stmt);
    return 0;
}

int plan_item_update(const plan_item_t *item) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "UPDATE plan_items SET plan_id=?, parent_id=?, title=?, item_type=?, planned_date=?, "
        "estimated_minutes=?, order_index=?, completion_rule=?, todo_id=?, actual_minutes=? WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, item->plan_id);
    sqlite3_bind_int64(stmt, 2, item->parent_id);
    sqlite3_bind_text(stmt, 3, item->title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 4, item->item_type);
    sqlite3_bind_int64(stmt, 5, item->planned_date);
    sqlite3_bind_int(stmt, 6, item->estimated_minutes);
    sqlite3_bind_int(stmt, 7, item->order_index);
    sqlite3_bind_int(stmt, 8, item->completion_rule);
    sqlite3_bind_int64(stmt, 9, item->todo_id);
    sqlite3_bind_int(stmt, 10, item->actual_minutes);
    sqlite3_bind_int64(stmt, 11, item->id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int plan_item_delete(int64_t id) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM plan_items WHERE id=?", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int plan_item_list_by_plan(int64_t plan_id, plan_item_t **items, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db,
        "SELECT id, plan_id, parent_id, title, item_type, planned_date, "
        "estimated_minutes, order_index, completion_rule, todo_id, actual_minutes "
        "FROM plan_items WHERE plan_id=? ORDER BY order_index",
        -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, plan_id);

    int cap = 16, cnt = 0;
    plan_item_t *list = malloc(cap * sizeof(plan_item_t));
    if (!list) { sqlite3_finalize(stmt); return -1; }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (cnt >= cap) {
            cap *= 2;
            plan_item_t *new_list = realloc(list, cap * sizeof(plan_item_t));
            if (!new_list) { free(list); sqlite3_finalize(stmt); return -1; }
            list = new_list;
        }
        memset(&list[cnt], 0, sizeof(plan_item_t));
        list[cnt].id = sqlite3_column_int64(stmt, 0);
        list[cnt].plan_id = sqlite3_column_int64(stmt, 1);
        list[cnt].parent_id = sqlite3_column_int64(stmt, 2);
        strncpy(list[cnt].title, (const char *)sqlite3_column_text(stmt, 3), ITEM_TITLE_MAX - 1);
        list[cnt].item_type = sqlite3_column_int(stmt, 4);
        list[cnt].planned_date = sqlite3_column_int64(stmt, 5);
        list[cnt].estimated_minutes = sqlite3_column_int(stmt, 6);
        list[cnt].order_index = sqlite3_column_int(stmt, 7);
        list[cnt].completion_rule = sqlite3_column_int(stmt, 8);
        list[cnt].todo_id = sqlite3_column_int64(stmt, 9);
        list[cnt].actual_minutes = sqlite3_column_int(stmt, 10);
        cnt++;
    }
    sqlite3_finalize(stmt);

    *items = list;
    *count = cnt;
    return 0;
}

void plan_item_list_free(plan_item_t *items, int count) {
    (void)count;
    if (items) free(items);
}

/* ============================================================
 * 批量操作
 * ============================================================ */
int todo_batch_update_status(const int64_t *ids, int count, const char *status) {
    sqlite3 *db = todo_db_handle();
    if (!db || !ids || count <= 0) return -1;

    char sql[4096];
    char placeholders[256] = "";
    int n = count > 100 ? 100 : count;
    for (int i = 0; i < n; i++) {
        if (i > 0) strcat(placeholders, ",");
        char buf[16];
        snprintf(buf, sizeof(buf), "?%d", i + 2);
        strcat(placeholders, buf);
    }
    snprintf(sql, sizeof(sql),
        "UPDATE todos SET status = ?1, updated_at = CAST(strftime('%s','now') AS INTEGER) WHERE id IN (%s)",
        placeholders);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, status, -1, SQLITE_STATIC);
    for (int i = 0; i < n; i++) {
        sqlite3_bind_int64(stmt, i + 2, ids[i]);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int todo_batch_delete(const int64_t *ids, int count) {
    sqlite3 *db = todo_db_handle();
    if (!db || !ids || count <= 0) return -1;

    char sql[4096];
    char placeholders[256] = "";
    int n = count > 100 ? 100 : count;
    for (int i = 0; i < n; i++) {
        if (i > 0) strcat(placeholders, ",");
        char buf[16];
        snprintf(buf, sizeof(buf), "?%d", i + 1);
        strcat(placeholders, buf);
    }
    snprintf(sql, sizeof(sql), "DELETE FROM todos WHERE id IN (%s)", placeholders);

    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    for (int i = 0; i < n; i++) {
        sqlite3_bind_int64(stmt, i + 1, ids[i]);
    }

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}