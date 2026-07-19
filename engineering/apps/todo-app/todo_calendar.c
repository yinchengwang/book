#include "todo_calendar.h"
#include "todo_model.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

/* 外部引用 todo_model.c 中的全局状态 */
extern todo_t *g_todos;
extern int g_todo_count;
extern int find_todo_idx(int64_t id);

/* 全局标记：有待确认的过期任务 */
static int        g_pending_cc_count = 0;
static int64_t   *g_pending_cc_ids = NULL;
static int        g_pending_cc_cap = 0;

/* 后台定时任务线程 */
static HANDLE g_timer_thread = NULL;
static volatile int g_timer_running = 0;

/* 获取今天的 Unix 时间戳（00:00:00 UTC） */
static int64_t today_start(void) {
    time_t now = time(NULL);
    return (int64_t)(now / 86400) * 86400;
}

/* 获取某天的 00:00:00 */
static int64_t day_start(int64_t ts) {
    return (ts / 86400) * 86400;
}

/* 获取某天是周几（1=周一, 7=周日） */
static int day_of_week(int64_t ts) {
    time_t t = (time_t)ts;
    struct tm *lt = localtime(&t);
    int wday = lt->tm_wday; /* 0=周日 */
    return wday == 0 ? 7 : wday;
}

/* 获取某周的周一 00:00:00 */
static int64_t week_start(int64_t ts) {
    int64_t ds = day_start(ts);
    int dow = day_of_week(ds);
    return ds - (int64_t)(dow - 1) * 86400;
}

/* 获取某月的 1 号 00:00:00 */
static int64_t month_start(int64_t ts) {
    time_t t = (time_t)ts;
    struct tm *lt = localtime(&t);
    struct tm copy = *lt;
    copy.tm_mday = 1;
    copy.tm_hour = 0; copy.tm_min = 0; copy.tm_sec = 0;
    return (int64_t)mktime(&copy);
}

/* 获取某月的天数 */
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

/* 计算到次日 00:00 的毫秒数 */
static int64_t ms_until_tomorrow(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    int64_t ms_passed = (int64_t)t->tm_hour * 3600000LL
                      + (int64_t)t->tm_min * 60000LL
                      + (int64_t)t->tm_sec * 1000LL;
    int64_t ms_remaining = 86400000LL - ms_passed;
    return ms_remaining > 0 ? ms_remaining : 1000;
}

/* 扫描所有过期任务，加入待确认列表 */
static void scan_overdue_todos(void) {
    int64_t now = today_start();
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") != 0) continue;
        if (g_todos[i].due_date <= 0) continue;
        if (g_todos[i].due_date >= now) continue; /* 未过期 */

        /* 检查是否已在待确认列表中 */
        int already = 0;
        for (int j = 0; j < g_pending_cc_count; j++) {
            if (g_pending_cc_ids[j] == g_todos[i].id) { already = 1; break; }
        }
        if (already) continue;

        /* 加入待确认列表 */
        if (g_pending_cc_count >= g_pending_cc_cap) {
            g_pending_cc_cap = g_pending_cc_cap ? g_pending_cc_cap * 2 : 64;
            int64_t *new_ids = (int64_t *)realloc(g_pending_cc_ids, g_pending_cc_cap * sizeof(int64_t));
            if (!new_ids) return;
            g_pending_cc_ids = new_ids;
        }
        g_pending_cc_ids[g_pending_cc_count++] = g_todos[i].id;
    }
}

/* 后台定时任务线程 */
static DWORD WINAPI timer_thread_func(LPVOID arg) {
    (void)arg;
    while (g_timer_running) {
        int64_t wait_ms = ms_until_tomorrow();
        Sleep((DWORD)wait_ms);
        if (!g_timer_running) break;
        scan_overdue_todos();
    }
    return 0;
}

void calendar_init(void) {
    g_pending_cc_count = 0;
    g_pending_cc_cap = 0;
    g_pending_cc_ids = NULL;
    g_timer_running = 0;
    g_timer_thread = NULL;
}

void calendar_timer_start(void) {
    if (g_timer_running) return;
    g_timer_running = 1;
    DWORD tid;
    g_timer_thread = CreateThread(NULL, 0, timer_thread_func, NULL, 0, &tid);
    /* 启动时立即扫描一次 */
    scan_overdue_todos();
}

void calendar_timer_stop(void) {
    g_timer_running = 0;
    if (g_timer_thread) {
        WaitForSingleObject(g_timer_thread, 3000);
        CloseHandle(g_timer_thread);
        g_timer_thread = NULL;
    }
}

void calendar_shutdown(void) {
    calendar_timer_stop();
    if (g_pending_cc_ids) {
        free(g_pending_cc_ids);
        g_pending_cc_ids = NULL;
    }
    g_pending_cc_count = 0;
    g_pending_cc_cap = 0;
}

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

int calendar_day_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ds = day_start(date);
    int64_t de = ds + 86400;

    /* 第一遍：统计符合条件的任务数 */
    int match_count = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") != 0) continue;
        if (g_todos[i].due_date < ds || g_todos[i].due_date >= de) continue;
        if (ts_id >= 0 && g_todos[i].task_system_id != ts_id) continue;
        match_count++;
    }

    if (match_count == 0) {
        *todos = NULL;
        *count = 0;
        return 0;
    }

    /* 分配数组 */
    *todos = (todo_t *)malloc(match_count * sizeof(todo_t));
    if (!*todos) return -1;

    /* 第二遍：复制匹配的任务 */
    int idx = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") != 0) continue;
        if (g_todos[i].due_date < ds || g_todos[i].due_date >= de) continue;
        if (ts_id >= 0 && g_todos[i].task_system_id != ts_id) continue;
        (*todos)[idx++] = g_todos[i];
    }

    /* 排序：按 original_date → priority → sort_order */
    qsort(*todos, match_count, sizeof(todo_t), todo_cmp);

    *count = match_count;
    return 0;
}

int calendar_week_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ws = week_start(date);
    int64_t we = ws + 7 * 86400;

    int match_count = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") != 0) continue;
        if (g_todos[i].due_date < ws || g_todos[i].due_date >= we) continue;
        if (ts_id >= 0 && g_todos[i].task_system_id != ts_id) continue;
        match_count++;
    }

    if (match_count == 0) {
        *todos = NULL;
        *count = 0;
        return 0;
    }

    *todos = (todo_t *)malloc(match_count * sizeof(todo_t));
    if (!*todos) return -1;

    int idx = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") != 0) continue;
        if (g_todos[i].due_date < ws || g_todos[i].due_date >= we) continue;
        if (ts_id >= 0 && g_todos[i].task_system_id != ts_id) continue;
        (*todos)[idx++] = g_todos[i];
    }

    qsort(*todos, match_count, sizeof(todo_t), todo_cmp);

    *count = match_count;
    return 0;
}

int calendar_month_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ms = month_start(date);
    int64_t me = ms + (int64_t)month_days(date) * 86400;

    int match_count = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") != 0) continue;
        if (g_todos[i].due_date < ms || g_todos[i].due_date >= me) continue;
        if (ts_id >= 0 && g_todos[i].task_system_id != ts_id) continue;
        match_count++;
    }

    if (match_count == 0) {
        *todos = NULL;
        *count = 0;
        return 0;
    }

    *todos = (todo_t *)malloc(match_count * sizeof(todo_t));
    if (!*todos) return -1;

    int idx = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") != 0) continue;
        if (g_todos[i].due_date < ms || g_todos[i].due_date >= me) continue;
        if (ts_id >= 0 && g_todos[i].task_system_id != ts_id) continue;
        (*todos)[idx++] = g_todos[i];
    }

    qsort(*todos, match_count, sizeof(todo_t), todo_cmp);

    *count = match_count;
    return 0;
}

int calendar_pending_carryover(todo_t **todos, int *count) {
    if (g_pending_cc_count == 0) {
        *todos = NULL;
        *count = 0;
        return 0;
    }
    *todos = (todo_t *)malloc(g_pending_cc_count * sizeof(todo_t));
    if (!*todos) return -1;
    *count = 0;
    for (int i = 0; i < g_pending_cc_count; i++) {
        int idx = find_todo_idx(g_pending_cc_ids[i]);
        if (idx >= 0) {
            (*todos)[*count] = g_todos[idx];
            (*count)++;
        }
    }
    return 0;
}

carryover_report_t calendar_confirm_carryover(carryover_confirm_t *items, int count) {
    carryover_report_t report = {0};
    int64_t now = today_start();
    for (int i = 0; i < count; i++) {
        int idx = find_todo_idx(items[i].todo_id);
        if (idx < 0) continue;

        if (items[i].completed) {
            /* 标记完成 */
            strcpy(g_todos[idx].status, "closed");
            g_todos[idx].completed_at = time(NULL);
        } else {
            /* 顺延到今天 */
            g_todos[idx].due_date = now;
            g_todos[idx].carryover_count++;
            report.carried_in_week++;
        }
        g_todos[idx].updated_at = time(NULL);
    }
    /* 清空待确认列表 */
    g_pending_cc_count = 0;
    todo_db_save();
    return report;
}

int calendar_postpone(int64_t todo_id, int days) {
    int idx = find_todo_idx(todo_id);
    if (idx < 0) return -1;
    g_todos[idx].due_date += (int64_t)days * 86400;
    g_todos[idx].postpone_until = g_todos[idx].due_date;
    g_todos[idx].updated_at = time(NULL);
    todo_db_save();
    return 0;
}

carryover_report_t calendar_promote(int scope) {
    (void)scope;
    carryover_report_t r = {0};
    return r;
}