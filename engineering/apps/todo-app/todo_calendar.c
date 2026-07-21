#include "todo_calendar.h"
#include "todo_model.h"
#include "todo_db.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <windows.h>
#include <stdio.h>
#include <sqlite3.h>

/* ============================================================
 * 辅助函数：时间计算
 * ============================================================ */

static int64_t today_start(void) {
    time_t now = time(NULL);
    return (int64_t)(now / 86400) * 86400;
}

static int64_t day_start(int64_t ts) {
    return (ts / 86400) * 86400;
}

static int day_of_week(int64_t ts) {
    time_t t = (time_t)ts;
    struct tm *lt = localtime(&t);
    int wday = lt->tm_wday;
    return wday == 0 ? 7 : wday;
}

static int64_t week_start(int64_t ts) {
    int64_t ds = day_start(ts);
    int dow = day_of_week(ds);
    return ds - (int64_t)(dow - 1) * 86400;
}

static int64_t month_start(int64_t ts) {
    time_t t = (time_t)ts;
    struct tm *lt = localtime(&t);
    struct tm copy = *lt;
    copy.tm_mday = 1;
    copy.tm_hour = 0; copy.tm_min = 0; copy.tm_sec = 0;
    return (int64_t)mktime(&copy);
}

static int month_days(int64_t ts) {
    time_t t = (time_t)ts;
    struct tm *lt = localtime(&t);
    struct tm copy = *lt;
    copy.tm_mon++;
    if (copy.tm_mon > 11) { copy.tm_mon = 0; copy.tm_year++; }
    copy.tm_mday = 1;
    copy.tm_hour = 0; copy.tm_min = 0; copy.tm_sec = 0;
    time_t next = mktime(&copy);
    return (int)((next - t) / 86400);
}

/* ============================================================
 * 日历查询：从 SQLite 直接查询，不再依赖 g_todos 全局数组
 * ============================================================ */

/* 按 original_date → priority → sort_order 排序 */
static int todo_cmp(const void *a, const void *b) {
    const todo_t *ta = (const todo_t *)a;
    const todo_t *tb = (const todo_t *)b;
    if (ta->original_date != tb->original_date)
        return ta->original_date < tb->original_date ? -1 : 1;
    if (ta->priority != tb->priority)
        return ta->priority < tb->priority ? -1 : 1;
    if (ta->sort_order != tb->sort_order)
        return ta->sort_order < tb->sort_order ? -1 : 1;
    return 0;
}

/* 从 SQLite 查询时间范围内的 todo */
static int query_todos_by_range(int64_t range_start, int64_t range_end,
                                 int64_t ts_id, todo_t **todos, int *count) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;
    sqlite3_stmt *stmt = NULL;

    const char *sql = "SELECT * FROM todos WHERE status='open' "
                      "AND due_date >= ? AND due_date < ? "
                      "AND (? < 0 OR task_system_id = ?) "
                      "ORDER BY original_date, priority, sort_order";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int64(stmt, 1, range_start);
    sqlite3_bind_int64(stmt, 2, range_end);
    sqlite3_bind_int64(stmt, 3, ts_id);
    sqlite3_bind_int64(stmt, 4, ts_id);

    /* 先统计数量 */
    int n = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) n++;
    sqlite3_reset(stmt);
    sqlite3_bind_int64(stmt, 1, range_start);
    sqlite3_bind_int64(stmt, 2, range_end);
    sqlite3_bind_int64(stmt, 3, ts_id);
    sqlite3_bind_int64(stmt, 4, ts_id);

    if (n == 0) {
        *todos = NULL;
        *count = 0;
        sqlite3_finalize(stmt);
        return 0;
    }

    todo_t *arr = (todo_t *)malloc(n * sizeof(todo_t));
    if (!arr) { sqlite3_finalize(stmt); return -1; }

    int i = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && i < n) {
        memset(&arr[i], 0, sizeof(arr[i]));
        arr[i].id = sqlite3_column_int64(stmt, 0);
        strncpy(arr[i].title, (const char *)sqlite3_column_text(stmt, 1), TODO_TITLE_MAX - 1);
        strncpy(arr[i].description, (const char *)sqlite3_column_text(stmt, 2), TODO_DESC_MAX - 1);
        strncpy(arr[i].status, (const char *)sqlite3_column_text(stmt, 3), TODO_STATUS_MAX - 1);
        const char *labels = (const char *)sqlite3_column_text(stmt, 4);
        strncpy(arr[i].labels, labels ? labels : "[]", TODO_LABELS_MAX - 1);
        arr[i].priority = sqlite3_column_int(stmt, 5);
        arr[i].due_date = sqlite3_column_int64(stmt, 6);
        arr[i].group_id = sqlite3_column_int64(stmt, 7);
        arr[i].sort_order = sqlite3_column_int(stmt, 8);
        arr[i].todo_type = sqlite3_column_int(stmt, 9);
        arr[i].original_date = sqlite3_column_int64(stmt, 10);
        arr[i].carryover_count = sqlite3_column_int(stmt, 11);
        arr[i].plan_id = sqlite3_column_int64(stmt, 12);
        arr[i].plan_item_id = sqlite3_column_int64(stmt, 13);
        arr[i].completed_at = sqlite3_column_int64(stmt, 14);
        arr[i].postpone_until = sqlite3_column_int64(stmt, 15);
        arr[i].task_system_id = sqlite3_column_int64(stmt, 16);
        arr[i].created_at = sqlite3_column_int64(stmt, 17);
        arr[i].updated_at = sqlite3_column_int64(stmt, 18);
        i++;
    }

    sqlite3_finalize(stmt);
    *todos = arr;
    *count = n;
    return 0;
}

int calendar_day_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ds = day_start(date);
    int64_t de = ds + 86400;
    return query_todos_by_range(ds, de, ts_id, todos, count);
}

int calendar_week_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ws = week_start(date);
    int64_t we = ws + 7 * 86400;
    return query_todos_by_range(ws, we, ts_id, todos, count);
}

int calendar_month_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ms = month_start(date);
    int64_t me = ms + (int64_t)month_days(date) * 86400;
    return query_todos_by_range(ms, me, ts_id, todos, count);
}

/* ============================================================
 * 顺延机制（基于 SQLite）
 * ============================================================ */

/* 全局标记：有待确认的过期任务 */
static int        g_pending_cc_count = 0;
static int64_t   *g_pending_cc_ids = NULL;
static int        g_pending_cc_cap = 0;

/* 扫描所有过期任务，加入待确认列表 */
static void scan_overdue_todos(void) {
    sqlite3 *db = todo_db_handle();
    if (!db) return;
    int64_t now = today_start();
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db,
        "SELECT id FROM todos WHERE status='open' AND due_date > 0 AND due_date < ?",
        -1, &stmt, NULL) != SQLITE_OK) return;
    sqlite3_bind_int64(stmt, 1, now);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int64_t id = sqlite3_column_int64(stmt, 0);
        /* 检查是否已在待确认列表中 */
        int already = 0;
        for (int j = 0; j < g_pending_cc_count; j++) {
            if (g_pending_cc_ids[j] == id) { already = 1; break; }
        }
        if (already) continue;

        if (g_pending_cc_count >= g_pending_cc_cap) {
            g_pending_cc_cap = g_pending_cc_cap ? g_pending_cc_cap * 2 : 64;
            int64_t *new_ids = (int64_t *)realloc(g_pending_cc_ids, g_pending_cc_cap * sizeof(int64_t));
            if (!new_ids) { sqlite3_finalize(stmt); return; }
            g_pending_cc_ids = new_ids;
        }
        g_pending_cc_ids[g_pending_cc_count++] = id;
    }
    sqlite3_finalize(stmt);
}

void calendar_init(void) {
    g_pending_cc_count = 0;
    g_pending_cc_cap = 0;
    g_pending_cc_ids = NULL;
}

void calendar_timer_start(void) {
    /* 在 Git Bash 环境下 CreateThread 有 segfault 问题，暂不启用 */
    scan_overdue_todos();
}

void calendar_timer_stop(void) {
    /* no-op */
}

void calendar_shutdown(void) {
    if (g_pending_cc_ids) {
        free(g_pending_cc_ids);
        g_pending_cc_ids = NULL;
    }
    g_pending_cc_count = 0;
    g_pending_cc_cap = 0;
}

int calendar_pending_carryover(todo_t **todos, int *count) {
    if (g_pending_cc_count == 0) {
        *todos = NULL;
        *count = 0;
        return 0;
    }

    todo_t *arr = (todo_t *)malloc(g_pending_cc_count * sizeof(todo_t));
    if (!arr) return -1;
    int found = 0;

    for (int i = 0; i < g_pending_cc_count; i++) {
        if (todo_get_by_id(g_pending_cc_ids[i], &arr[found]) == 0)
            found++;
    }

    *todos = arr;
    *count = found;
    return 0;
}

carryover_report_t calendar_confirm_carryover(carryover_confirm_t *items, int count) {
    carryover_report_t report = {0};
    int64_t now = today_start();
    sqlite3 *db = todo_db_handle();
    if (!db) return report;

    for (int i = 0; i < count; i++) {
        if (items[i].completed) {
            char sql[256];
            snprintf(sql, sizeof(sql),
                "UPDATE todos SET status='closed', completed_at=CAST(strftime('%%s','now') AS INTEGER), "
                "updated_at=CAST(strftime('%%s','now') AS INTEGER) WHERE id=?");
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, items[i].todo_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        } else {
            /* 顺延到今天 */
            sqlite3_stmt *stmt = NULL;
            if (sqlite3_prepare_v2(db,
                "UPDATE todos SET due_date=?, carryover_count=carryover_count+1, "
                "updated_at=CAST(strftime('%s','now') AS INTEGER) WHERE id=?",
                -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, now);
                sqlite3_bind_int64(stmt, 2, items[i].todo_id);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
            report.carried_in_week++;
        }
    }

    /* 清空待确认列表 */
    g_pending_cc_count = 0;
    return report;
}

int calendar_postpone(int64_t todo_id, int days) {
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;
    sqlite3_stmt *stmt = NULL;

    if (sqlite3_prepare_v2(db,
        "UPDATE todos SET due_date=due_date+?, postpone_until=due_date+?, "
        "updated_at=CAST(strftime('%s','now') AS INTEGER) WHERE id=?",
        -1, &stmt, NULL) != SQLITE_OK) return -1;

    int64_t offset = (int64_t)days * 86400;
    sqlite3_bind_int64(stmt, 1, offset);
    sqlite3_bind_int64(stmt, 2, offset);
    sqlite3_bind_int64(stmt, 3, todo_id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE && sqlite3_changes(db) > 0) ? 0 : -1;
}

carryover_report_t calendar_promote(int scope) {
    (void)scope;
    carryover_report_t r = {0};
    return r;
}