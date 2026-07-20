#include "todo_db.h"
#include "todo_model.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

sqlite3 *g_db = NULL;

/* ============================================================
 * 内部辅助函数
 * ============================================================ */

static int exec_sql(sqlite3 *db, const char *sql) {
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite error: %s\nSQL: %s\n", err ? err : "unknown", sql);
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

/* ============================================================
 * 建表
 * ============================================================ */

static int create_all_tables(sqlite3 *db) {
    /* 拆分为多个 exec_sql 调用，避免超长字符串 */
    const char *sql1 =
        "CREATE TABLE IF NOT EXISTS todos ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  title           TEXT NOT NULL DEFAULT '',"
        "  description     TEXT NOT NULL DEFAULT '',"
        "  status          TEXT NOT NULL DEFAULT 'open',"
        "  labels          TEXT NOT NULL DEFAULT '[]',"
        "  priority        INTEGER NOT NULL DEFAULT 2,"
        "  due_date        INTEGER NOT NULL DEFAULT 0,"
        "  group_id        INTEGER NOT NULL DEFAULT 0,"
        "  sort_order      INTEGER NOT NULL DEFAULT 0,"
        "  todo_type       INTEGER NOT NULL DEFAULT 0,"
        "  original_date   INTEGER NOT NULL DEFAULT 0,"
        "  carryover_count INTEGER NOT NULL DEFAULT 0,"
        "  plan_id         INTEGER NOT NULL DEFAULT 0,"
        "  plan_item_id    INTEGER NOT NULL DEFAULT 0,"
        "  completed_at    INTEGER NOT NULL DEFAULT 0,"
        "  postpone_until  INTEGER NOT NULL DEFAULT 0,"
        "  task_system_id  INTEGER NOT NULL DEFAULT 0,"
        "  created_at      INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER)),"
        "  updated_at      INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))"
        ");";
    if (exec_sql(db, sql1) != 0) return -1;

    const char *sql_idx =
        "CREATE INDEX IF NOT EXISTS idx_todos_status    ON todos(status);"
        "CREATE INDEX IF NOT EXISTS idx_todos_group     ON todos(group_id);"
        "CREATE INDEX IF NOT EXISTS idx_todos_due_date  ON todos(due_date);"
        "CREATE INDEX IF NOT EXISTS idx_todos_plan      ON todos(plan_id);"
        "CREATE INDEX IF NOT EXISTS idx_todos_ts        ON todos(task_system_id);";
    if (exec_sql(db, sql_idx) != 0) return -1;

    const char *sql2 =
        "CREATE TABLE IF NOT EXISTS fields_def ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name        TEXT NOT NULL,"
        "  type        TEXT NOT NULL,"
        "  options     TEXT NOT NULL DEFAULT '{}',"
        "  built_in    INTEGER NOT NULL DEFAULT 0,"
        "  ref_table   TEXT DEFAULT NULL,"
        "  ref_field   TEXT DEFAULT NULL,"
        "  formula     TEXT DEFAULT NULL,"
        "  sort_order  INTEGER NOT NULL DEFAULT 0,"
        "  created_at  INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_fields_order ON fields_def(sort_order);";
    if (exec_sql(db, sql2) != 0) return -1;

    const char *sql3 =
        "CREATE TABLE IF NOT EXISTS field_values ("
        "  todo_id  INTEGER NOT NULL,"
        "  field_id INTEGER NOT NULL,"
        "  value    TEXT,"
        "  PRIMARY KEY (todo_id, field_id),"
        "  FOREIGN KEY (todo_id)  REFERENCES todos(id)       ON DELETE CASCADE,"
        "  FOREIGN KEY (field_id) REFERENCES fields_def(id)  ON DELETE CASCADE"
        ") WITHOUT ROWID;"
        "CREATE INDEX IF NOT EXISTS idx_fv_field ON field_values(field_id);";
    if (exec_sql(db, sql3) != 0) return -1;

    const char *sql4 =
        "CREATE TABLE IF NOT EXISTS checklist ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  todo_id    INTEGER NOT NULL,"
        "  text       TEXT NOT NULL,"
        "  done       INTEGER NOT NULL DEFAULT 0,"
        "  sort_order INTEGER NOT NULL DEFAULT 0,"
        "  FOREIGN KEY (todo_id) REFERENCES todos(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_check_todo ON checklist(todo_id);"
        "CREATE TABLE IF NOT EXISTS groups_t ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT NOT NULL,"
        "  color      TEXT NOT NULL DEFAULT '#4A90D9',"
        "  sort_order INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))"
        ");"
        "CREATE TABLE IF NOT EXISTS comments ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  todo_id    INTEGER NOT NULL,"
        "  text       TEXT NOT NULL,"
        "  created_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER)),"
        "  FOREIGN KEY (todo_id) REFERENCES todos(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_comment_todo ON comments(todo_id);"
        "CREATE TABLE IF NOT EXISTS task_systems ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name        TEXT NOT NULL,"
        "  description TEXT NOT NULL DEFAULT '',"
        "  color       TEXT NOT NULL DEFAULT '#4A90D9',"
        "  sort_order  INTEGER NOT NULL DEFAULT 0,"
        "  created_at  INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))"
        ");";
    if (exec_sql(db, sql4) != 0) return -1;

    const char *sql5 =
        "CREATE TABLE IF NOT EXISTS plans ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name        TEXT NOT NULL,"
        "  description TEXT NOT NULL DEFAULT '',"
        "  start_date  INTEGER NOT NULL DEFAULT 0,"
        "  end_date    INTEGER NOT NULL DEFAULT 0,"
        "  color       TEXT NOT NULL DEFAULT '#4A90D9',"
        "  status      INTEGER NOT NULL DEFAULT 0,"
        "  created_at  INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER)),"
        "  updated_at  INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))"
        ");"
        "CREATE TABLE IF NOT EXISTS plan_items ("
        "  id                INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  plan_id           INTEGER NOT NULL,"
        "  parent_id         INTEGER NOT NULL DEFAULT 0,"
        "  title             TEXT NOT NULL,"
        "  item_type         INTEGER NOT NULL DEFAULT 3,"
        "  planned_date      INTEGER NOT NULL DEFAULT 0,"
        "  estimated_minutes INTEGER NOT NULL DEFAULT 0,"
        "  order_index       INTEGER NOT NULL DEFAULT 0,"
        "  completion_rule   INTEGER NOT NULL DEFAULT 1,"
        "  todo_id           INTEGER NOT NULL DEFAULT 0,"
        "  actual_minutes    INTEGER NOT NULL DEFAULT 0,"
        "  FOREIGN KEY (plan_id) REFERENCES plans(id) ON DELETE CASCADE"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_pi_plan ON plan_items(plan_id);";
    if (exec_sql(db, sql5) != 0) return -1;

    const char *sql6 =
        "CREATE TABLE IF NOT EXISTS meta ("
        "  key   TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL"
        ");"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('schema_version', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_todo_id', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_check_id', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_group_id', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_comment_id', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_ts_id', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_plan_id', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_pi_id', '1');"
        "INSERT OR IGNORE INTO meta (key, value) VALUES ('next_field_id', '10');";
    if (exec_sql(db, sql6) != 0) return -1;

    /* 视图表 */
    const char *sql7 =
        "CREATE TABLE IF NOT EXISTS views ("
        "  id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  name       TEXT NOT NULL,"
        "  type       TEXT NOT NULL DEFAULT 'table',"
        "  config     TEXT NOT NULL DEFAULT '{}',"
        "  is_default INTEGER NOT NULL DEFAULT 0,"
        "  sort_order INTEGER NOT NULL DEFAULT 0,"
        "  created_at INTEGER NOT NULL DEFAULT (CAST(strftime('%s','now') AS INTEGER))"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_views_order ON views(sort_order);";
    if (exec_sql(db, sql7) != 0) return -1;

    return 0;
}

/* ============================================================
 * 预置内置字段（id=1-9）
 * ============================================================ */

static int insert_builtin_fields(sqlite3 *db) {
    const char *sql =
        "INSERT OR IGNORE INTO fields_def (id, name, type, options, built_in, sort_order) VALUES "
        "(1, '标题', 'text', '{}', 1, 1),"
        "(2, '描述', 'text', '{}', 1, 2),"
        "(3, '状态', 'single_select', '{\"choices\":[\"open\",\"closed\",\"archived\"]}', 1, 3),"
        "(4, '优先级', 'single_select', '{\"choices\":[\"紧急\",\"高\",\"中\",\"低\",\"无\"]}', 1, 4),"
        "(5, '截止日期', 'date', '{}', 1, 5),"
        "(6, '标签', 'multi_select', '{}', 1, 6),"
        "(7, '分组', 'single_select', '{}', 1, 7),"
        "(8, '创建时间', 'datetime', '{}', 1, 8),"
        "(9, '更新时间', 'datetime', '{}', 1, 9);";
    return exec_sql(db, sql);
}

/* ============================================================
 * 预置默认视图
 * ============================================================ */

static int insert_default_views(sqlite3 *db) {
    /* 检查是否已有视图 */
    sqlite3_stmt *check = NULL;
    if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM views", -1, &check, NULL) == SQLITE_OK) {
        if (sqlite3_step(check) == SQLITE_ROW && sqlite3_column_int(check, 0) > 0) {
            sqlite3_finalize(check);
            return 0;  /* 已有视图，跳过 */
        }
        sqlite3_finalize(check);
    }

    /* 预置 4 个默认视图 */
    const char *sql =
        "INSERT INTO views (name, type, config, is_default, sort_order) VALUES "
        "('表格视图', 'table', '{\"visible_fields\":[1,3,4,5],\"group_by\":null,\"sort_by\":{\"field\":\"5\",\"desc\":false}}', 1, 1),"
        "('看板视图', 'board', '{\"group_field\":\"3\",\"card_fields\":[1,5],\"card_cover_field\":null}', 0, 2),"
        "('日历视图', 'calendar', '{\"date_field\":\"5\",\"show_fields\":[1,3]}', 0, 3),"
        "('甘特图', 'gantt', '{\"start_field\":\"5\",\"end_field\":null,\"duration_field\":null,\"bar_label_field\":\"1\"}', 0, 4);";
    return exec_sql(db, sql);
}

/* ============================================================
 * JSON→SQLite 迁移函数
 * ============================================================ */

int todo_db_migrate_from_json(const char *json_path) {
    if (!g_db) return -1;

    /* 读取 JSON 文件 */
    FILE *fp = fopen(json_path, "rb");
    if (!fp) {
        fprintf(stderr, "JSON 迁移：无法打开 %s\n", json_path);
        return -1;
    }
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *json_buf = (char *)malloc(fsize + 1);
    if (!json_buf) { fclose(fp); return -1; }
    fread(json_buf, 1, fsize, fp);
    json_buf[fsize] = '\0';
    fclose(fp);

    /* 解析 JSON */
    cJSON *root = cJSON_Parse(json_buf);
    free(json_buf);
    if (!root) return -1;

    /* 开始事务 */
    exec_sql(g_db, "BEGIN TRANSACTION;");

    /* ---- 迁移 todos ---- */
    cJSON *jitems = cJSON_GetObjectItem(root, "todos");
    if (jitems && cJSON_IsArray(jitems)) {
        int n = cJSON_GetArraySize(jitems);
        for (int i = 0; i < n; i++) {
            cJSON *o = cJSON_GetArrayItem(jitems, i);
            sqlite3_stmt *stmt = NULL;
            const char *sql = "INSERT INTO todos (id, title, description, status, labels, priority, "
                "due_date, group_id, sort_order, todo_type, original_date, carryover_count, "
                "plan_id, plan_item_id, completed_at, postpone_until, task_system_id, "
                "created_at, updated_at) "
                "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19)";
            if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) continue;

            cJSON *jid = cJSON_GetObjectItem(o, "id");
            sqlite3_bind_int64(stmt, 1, jid ? (int64_t)jid->valuedouble : 0);

            cJSON *jtitle = cJSON_GetObjectItem(o, "title");
            sqlite3_bind_text(stmt, 2, jtitle && cJSON_IsString(jtitle) ? jtitle->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jdesc = cJSON_GetObjectItem(o, "description");
            sqlite3_bind_text(stmt, 3, jdesc && cJSON_IsString(jdesc) ? jdesc->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jstatus = cJSON_GetObjectItem(o, "status");
            sqlite3_bind_text(stmt, 4, jstatus && cJSON_IsString(jstatus) ? jstatus->valuestring : "open", -1, SQLITE_TRANSIENT);

            /* labels: 如果是数组则序列化 */
            cJSON *jlabels = cJSON_GetObjectItem(o, "labels");
            if (jlabels && cJSON_IsArray(jlabels)) {
                char *ls = cJSON_PrintUnformatted(jlabels);
                sqlite3_bind_text(stmt, 5, ls ? ls : "[]", -1, free);
            } else if (jlabels && cJSON_IsString(jlabels)) {
                sqlite3_bind_text(stmt, 5, jlabels->valuestring, -1, SQLITE_TRANSIENT);
            } else {
                sqlite3_bind_text(stmt, 5, "[]", -1, SQLITE_STATIC);
            }

            cJSON *jpriority = cJSON_GetObjectItem(o, "priority");
            sqlite3_bind_int(stmt, 6, jpriority ? (int)jpriority->valuedouble : 2);

            cJSON *jdue = cJSON_GetObjectItem(o, "due_date");
            sqlite3_bind_int64(stmt, 7, jdue ? (int64_t)jdue->valuedouble : 0);

            cJSON *jgid = cJSON_GetObjectItem(o, "group_id");
            sqlite3_bind_int64(stmt, 8, jgid ? (int64_t)jgid->valuedouble : 0);

            cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
            sqlite3_bind_int(stmt, 9, jso ? (int)jso->valuedouble : 0);

            cJSON *jtodo_type = cJSON_GetObjectItem(o, "todo_type");
            sqlite3_bind_int(stmt, 10, jtodo_type ? (int)jtodo_type->valuedouble : 0);

            cJSON *jorig = cJSON_GetObjectItem(o, "original_date");
            sqlite3_bind_int64(stmt, 11, jorig ? (int64_t)jorig->valuedouble : 0);

            cJSON *jcarry = cJSON_GetObjectItem(o, "carryover_count");
            sqlite3_bind_int(stmt, 12, jcarry ? (int)jcarry->valuedouble : 0);

            cJSON *jplan_id = cJSON_GetObjectItem(o, "plan_id");
            sqlite3_bind_int64(stmt, 13, jplan_id ? (int64_t)jplan_id->valuedouble : 0);

            cJSON *jpi_id = cJSON_GetObjectItem(o, "plan_item_id");
            sqlite3_bind_int64(stmt, 14, jpi_id ? (int64_t)jpi_id->valuedouble : 0);

            cJSON *jcompleted = cJSON_GetObjectItem(o, "completed_at");
            sqlite3_bind_int64(stmt, 15, jcompleted ? (int64_t)jcompleted->valuedouble : 0);

            cJSON *jpost = cJSON_GetObjectItem(o, "postpone_until");
            sqlite3_bind_int64(stmt, 16, jpost ? (int64_t)jpost->valuedouble : 0);

            cJSON *jtsid = cJSON_GetObjectItem(o, "task_system_id");
            sqlite3_bind_int64(stmt, 17, jtsid ? (int64_t)jtsid->valuedouble : 0);

            cJSON *jca = cJSON_GetObjectItem(o, "created_at");
            sqlite3_bind_int64(stmt, 18, jca ? (int64_t)jca->valuedouble : (int64_t)time(NULL));

            cJSON *jua = cJSON_GetObjectItem(o, "updated_at");
            sqlite3_bind_int64(stmt, 19, jua ? (int64_t)jua->valuedouble : (int64_t)time(NULL));

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    /* ---- 迁移 checklist ---- */
    cJSON *jchecks = cJSON_GetObjectItem(root, "checklist");
    if (jchecks && cJSON_IsArray(jchecks)) {
        int n = cJSON_GetArraySize(jchecks);
        for (int i = 0; i < n; i++) {
            cJSON *o = cJSON_GetArrayItem(jchecks, i);
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g_db,
                "INSERT INTO checklist (id, todo_id, text, done, sort_order) VALUES (?1,?2,?3,?4,?5)",
                -1, &stmt, NULL) != SQLITE_OK) continue;

            cJSON *jid = cJSON_GetObjectItem(o, "id");
            sqlite3_bind_int64(stmt, 1, jid ? (int64_t)jid->valuedouble : 0);

            cJSON *jtid = cJSON_GetObjectItem(o, "todo_id");
            if (!jtid) jtid = cJSON_GetObjectItem(o, "issue_id");
            sqlite3_bind_int64(stmt, 2, jtid ? (int64_t)jtid->valuedouble : 0);

            cJSON *jtext = cJSON_GetObjectItem(o, "text");
            sqlite3_bind_text(stmt, 3, jtext && cJSON_IsString(jtext) ? jtext->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jdone = cJSON_GetObjectItem(o, "done");
            sqlite3_bind_int(stmt, 4, jdone ? jdone->valueint : 0);

            cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
            sqlite3_bind_int(stmt, 5, jso ? jso->valueint : 0);

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    /* ---- 迁移 groups ---- */
    cJSON *jgroups = cJSON_GetObjectItem(root, "groups");
    if (jgroups && cJSON_IsArray(jgroups)) {
        int n = cJSON_GetArraySize(jgroups);
        for (int i = 0; i < n; i++) {
            cJSON *o = cJSON_GetArrayItem(jgroups, i);
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g_db,
                "INSERT INTO groups_t (id, name, color, sort_order, created_at) VALUES (?1,?2,?3,?4,?5)",
                -1, &stmt, NULL) != SQLITE_OK) continue;

            cJSON *jid = cJSON_GetObjectItem(o, "id");
            sqlite3_bind_int64(stmt, 1, jid ? (int64_t)jid->valuedouble : 0);

            cJSON *jname = cJSON_GetObjectItem(o, "name");
            sqlite3_bind_text(stmt, 2, jname && cJSON_IsString(jname) ? jname->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jcolor = cJSON_GetObjectItem(o, "color");
            sqlite3_bind_text(stmt, 3, jcolor && cJSON_IsString(jcolor) ? jcolor->valuestring : "#4A90D9", -1, SQLITE_TRANSIENT);

            cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
            sqlite3_bind_int(stmt, 4, jso ? jso->valueint : 0);

            cJSON *jca = cJSON_GetObjectItem(o, "created_at");
            sqlite3_bind_int64(stmt, 5, jca ? (int64_t)jca->valuedouble : (int64_t)time(NULL));

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    /* ---- 迁移 comments ---- */
    cJSON *jcomments = cJSON_GetObjectItem(root, "comments");
    if (jcomments && cJSON_IsArray(jcomments)) {
        int n = cJSON_GetArraySize(jcomments);
        for (int i = 0; i < n; i++) {
            cJSON *o = cJSON_GetArrayItem(jcomments, i);
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g_db,
                "INSERT INTO comments (id, todo_id, text, created_at) VALUES (?1,?2,?3,?4)",
                -1, &stmt, NULL) != SQLITE_OK) continue;

            cJSON *jid = cJSON_GetObjectItem(o, "id");
            sqlite3_bind_int64(stmt, 1, jid ? (int64_t)jid->valuedouble : 0);

            cJSON *jtid = cJSON_GetObjectItem(o, "todo_id");
            sqlite3_bind_int64(stmt, 2, jtid ? (int64_t)jtid->valuedouble : 0);

            cJSON *jtext = cJSON_GetObjectItem(o, "text");
            sqlite3_bind_text(stmt, 3, jtext && cJSON_IsString(jtext) ? jtext->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jca = cJSON_GetObjectItem(o, "created_at");
            sqlite3_bind_int64(stmt, 4, jca ? (int64_t)jca->valuedouble : (int64_t)time(NULL));

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    /* ---- 迁移 task_systems ---- */
    cJSON *jts = cJSON_GetObjectItem(root, "task_systems");
    if (jts && cJSON_IsArray(jts)) {
        int n = cJSON_GetArraySize(jts);
        for (int i = 0; i < n; i++) {
            cJSON *o = cJSON_GetArrayItem(jts, i);
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g_db,
                "INSERT INTO task_systems (id, name, description, color, sort_order, created_at) VALUES (?1,?2,?3,?4,?5,?6)",
                -1, &stmt, NULL) != SQLITE_OK) continue;

            cJSON *jid = cJSON_GetObjectItem(o, "id");
            sqlite3_bind_int64(stmt, 1, jid ? (int64_t)jid->valuedouble : 0);

            cJSON *jname = cJSON_GetObjectItem(o, "name");
            sqlite3_bind_text(stmt, 2, jname && cJSON_IsString(jname) ? jname->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jdesc = cJSON_GetObjectItem(o, "description");
            sqlite3_bind_text(stmt, 3, jdesc && cJSON_IsString(jdesc) ? jdesc->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jcolor = cJSON_GetObjectItem(o, "color");
            sqlite3_bind_text(stmt, 4, jcolor && cJSON_IsString(jcolor) ? jcolor->valuestring : "#4A90D9", -1, SQLITE_TRANSIENT);

            cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
            sqlite3_bind_int(stmt, 5, jso ? jso->valueint : 0);

            cJSON *jca = cJSON_GetObjectItem(o, "created_at");
            sqlite3_bind_int64(stmt, 6, jca ? (int64_t)jca->valuedouble : (int64_t)time(NULL));

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    /* ---- 迁移 plans ---- */
    cJSON *jplans = cJSON_GetObjectItem(root, "plans");
    if (jplans && cJSON_IsArray(jplans)) {
        int n = cJSON_GetArraySize(jplans);
        for (int i = 0; i < n; i++) {
            cJSON *o = cJSON_GetArrayItem(jplans, i);
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g_db,
                "INSERT INTO plans (id, name, description, start_date, end_date, color, status, created_at, updated_at) "
                "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9)",
                -1, &stmt, NULL) != SQLITE_OK) continue;

            cJSON *jid = cJSON_GetObjectItem(o, "id");
            sqlite3_bind_int64(stmt, 1, jid ? (int64_t)jid->valuedouble : 0);

            cJSON *jname = cJSON_GetObjectItem(o, "name");
            sqlite3_bind_text(stmt, 2, jname && cJSON_IsString(jname) ? jname->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jdesc = cJSON_GetObjectItem(o, "description");
            sqlite3_bind_text(stmt, 3, jdesc && cJSON_IsString(jdesc) ? jdesc->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jstart = cJSON_GetObjectItem(o, "start_date");
            sqlite3_bind_int64(stmt, 4, jstart ? (int64_t)jstart->valuedouble : 0);

            cJSON *jend = cJSON_GetObjectItem(o, "end_date");
            sqlite3_bind_int64(stmt, 5, jend ? (int64_t)jend->valuedouble : 0);

            cJSON *jcolor = cJSON_GetObjectItem(o, "color");
            sqlite3_bind_text(stmt, 6, jcolor && cJSON_IsString(jcolor) ? jcolor->valuestring : "#4A90D9", -1, SQLITE_TRANSIENT);

            cJSON *jstatus = cJSON_GetObjectItem(o, "status");
            sqlite3_bind_int(stmt, 7, jstatus ? jstatus->valueint : 0);

            cJSON *jca = cJSON_GetObjectItem(o, "created_at");
            sqlite3_bind_int64(stmt, 8, jca ? (int64_t)jca->valuedouble : (int64_t)time(NULL));

            cJSON *jua = cJSON_GetObjectItem(o, "updated_at");
            sqlite3_bind_int64(stmt, 9, jua ? (int64_t)jua->valuedouble : (int64_t)time(NULL));

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    /* ---- 迁移 plan_items ---- */
    cJSON *jpis = cJSON_GetObjectItem(root, "plan_items");
    if (jpis && cJSON_IsArray(jpis)) {
        int n = cJSON_GetArraySize(jpis);
        for (int i = 0; i < n; i++) {
            cJSON *o = cJSON_GetArrayItem(jpis, i);
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(g_db,
                "INSERT INTO plan_items (id, plan_id, parent_id, title, item_type, planned_date, "
                "estimated_minutes, order_index, completion_rule, todo_id, actual_minutes) "
                "VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11)",
                -1, &stmt, NULL) != SQLITE_OK) continue;

            cJSON *jid = cJSON_GetObjectItem(o, "id");
            sqlite3_bind_int64(stmt, 1, jid ? (int64_t)jid->valuedouble : 0);

            cJSON *jpid = cJSON_GetObjectItem(o, "plan_id");
            sqlite3_bind_int64(stmt, 2, jpid ? (int64_t)jpid->valuedouble : 0);

            cJSON *jparent = cJSON_GetObjectItem(o, "parent_id");
            sqlite3_bind_int64(stmt, 3, jparent ? (int64_t)jparent->valuedouble : 0);

            cJSON *jtitle = cJSON_GetObjectItem(o, "title");
            sqlite3_bind_text(stmt, 4, jtitle && cJSON_IsString(jtitle) ? jtitle->valuestring : "", -1, SQLITE_TRANSIENT);

            cJSON *jtype = cJSON_GetObjectItem(o, "item_type");
            sqlite3_bind_int(stmt, 5, jtype ? jtype->valueint : 3);

            cJSON *jpd = cJSON_GetObjectItem(o, "planned_date");
            sqlite3_bind_int64(stmt, 6, jpd ? (int64_t)jpd->valuedouble : 0);

            cJSON *jest = cJSON_GetObjectItem(o, "estimated_minutes");
            sqlite3_bind_int(stmt, 7, jest ? jest->valueint : 0);

            cJSON *joi = cJSON_GetObjectItem(o, "order_index");
            sqlite3_bind_int(stmt, 8, joi ? joi->valueint : 0);

            cJSON *jcr = cJSON_GetObjectItem(o, "completion_rule");
            sqlite3_bind_int(stmt, 9, jcr ? jcr->valueint : 1);

            cJSON *jti = cJSON_GetObjectItem(o, "todo_id");
            sqlite3_bind_int64(stmt, 10, jti ? (int64_t)jti->valuedouble : 0);

            cJSON *jam = cJSON_GetObjectItem(o, "actual_minutes");
            sqlite3_bind_int(stmt, 11, jam ? jam->valueint : 0);

            sqlite3_step(stmt);
            sqlite3_finalize(stmt);
        }
    }

    /* ---- 更新自增 ID 元数据 ---- */
    cJSON *jnext = cJSON_GetObjectItem(root, "next_todo_id");
    if (jnext) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)jnext->valuedouble);
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('next_todo_id', ?)", -1, &s, NULL);
        sqlite3_bind_text(s, 1, buf, -1, SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }
    jnext = cJSON_GetObjectItem(root, "next_check_id");
    if (jnext) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)jnext->valuedouble);
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('next_check_id', ?)", -1, &s, NULL);
        sqlite3_bind_text(s, 1, buf, -1, SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }
    jnext = cJSON_GetObjectItem(root, "next_group_id");
    if (jnext) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)jnext->valuedouble);
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('next_group_id', ?)", -1, &s, NULL);
        sqlite3_bind_text(s, 1, buf, -1, SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }
    jnext = cJSON_GetObjectItem(root, "next_comment_id");
    if (jnext) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)jnext->valuedouble);
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('next_comment_id', ?)", -1, &s, NULL);
        sqlite3_bind_text(s, 1, buf, -1, SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }
    jnext = cJSON_GetObjectItem(root, "next_ts_id");
    if (jnext) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)jnext->valuedouble);
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('next_ts_id', ?)", -1, &s, NULL);
        sqlite3_bind_text(s, 1, buf, -1, SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }
    jnext = cJSON_GetObjectItem(root, "next_plan_id");
    if (jnext) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)jnext->valuedouble);
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('next_plan_id', ?)", -1, &s, NULL);
        sqlite3_bind_text(s, 1, buf, -1, SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }
    jnext = cJSON_GetObjectItem(root, "next_pi_id");
    if (jnext) {
        char buf[32]; snprintf(buf, sizeof(buf), "%lld", (long long)jnext->valuedouble);
        sqlite3_stmt *s = NULL;
        sqlite3_prepare_v2(g_db, "INSERT OR REPLACE INTO meta (key, value) VALUES ('next_pi_id', ?)", -1, &s, NULL);
        sqlite3_bind_text(s, 1, buf, -1, SQLITE_STATIC);
        sqlite3_step(s); sqlite3_finalize(s);
    }

    /* 提交事务 */
    exec_sql(g_db, "COMMIT;");
    cJSON_Delete(root);

    /* 重命名 JSON 文件为 .bak */
    char bak_path[1024];
    snprintf(bak_path, sizeof(bak_path), "%s.bak", json_path);
    if (rename(json_path, bak_path) != 0) {
        fprintf(stderr, "JSON 迁移：重命名 %s -> %s 失败\n", json_path, bak_path);
    }

    printf("JSON 迁移完成：%s -> SQLite\n", json_path);
    return 0;
}

/* ============================================================
 * 开放/关闭数据库
 * ============================================================ */

int todo_db_open(const char *path) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }

    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQLite 打开失败: %s\n", sqlite3_errmsg(g_db));
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }

    /* 启用 WAL 模式 */
    exec_sql(g_db, "PRAGMA journal_mode=WAL;");
    /* 启用外键 */
    exec_sql(g_db, "PRAGMA foreign_keys=ON;");

    /* 建表 */
    if (create_all_tables(g_db) != 0) {
        fprintf(stderr, "建表失败\n");
        todo_db_close();
        return -1;
    }

    /* 预置内置字段 */
    if (insert_builtin_fields(g_db) != 0) {
        fprintf(stderr, "预置内置字段失败\n");
        todo_db_close();
        return -1;
    }

    /* 预置默认视图 */
    if (insert_default_views(g_db) != 0) {
        fprintf(stderr, "预置默认视图失败\n");
        todo_db_close();
        return -1;
    }

    return 0;
}

void todo_db_close(void) {
    if (g_db) {
        sqlite3_close(g_db);
        g_db = NULL;
    }
}

sqlite3 *todo_db_handle(void) {
    return g_db;
}

/* ============================================================
 * 元数据读写
 * ============================================================ */

int64_t todo_db_get_meta_int64(const char *key) {
    if (!g_db) return 0;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT value FROM meta WHERE key = ?";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    int64_t val = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *text = (const char *)sqlite3_column_text(stmt, 0);
        val = text ? (int64_t)atoll(text) : 0;
    }
    sqlite3_finalize(stmt);
    return val;
}

int todo_db_set_meta_int64(const char *key, int64_t value) {
    if (!g_db) return -1;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR REPLACE INTO meta (key, value) VALUES (?, ?)";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    sqlite3_bind_text(stmt, 2, buf, -1, SQLITE_STATIC);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int todo_db_table_exists(const char *table_name) {
    if (!g_db) return 0;
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT count(*) FROM sqlite_master WHERE type='table' AND name=?";
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, table_name, -1, SQLITE_STATIC);
    int exists = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        exists = sqlite3_column_int(stmt, 0) > 0;
    }
    sqlite3_finalize(stmt);
    return exists;
}