#include "todo_calendar.h"
#include "todo_model.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <windows.h>

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

void calendar_init(void) {
    g_pending_cc_count = 0;
    g_pending_cc_cap = 0;
    g_pending_cc_ids = NULL;
    g_timer_running = 0;
    g_timer_thread = NULL;
}

void calendar_timer_start(void) {
    (void)NULL;
}

void calendar_timer_stop(void) {
    (void)NULL;
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

int calendar_day_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    *todos = NULL; *count = 0; return 0;
}

int calendar_week_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    *todos = NULL; *count = 0; return 0;
}

int calendar_month_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    *todos = NULL; *count = 0; return 0;
}

int calendar_pending_carryover(todo_t **todos, int *count) {
    *todos = NULL; *count = 0; return 0;
}

carryover_report_t calendar_confirm_carryover(carryover_confirm_t *items, int count) {
    carryover_report_t r = {0};
    (void)items; (void)count;
    return r;
}

int calendar_postpone(int64_t todo_id, int days) {
    (void)todo_id; (void)days;
    return 0;
}

carryover_report_t calendar_promote(int scope) {
    (void)scope;
    carryover_report_t r = {0};
    return r;
}