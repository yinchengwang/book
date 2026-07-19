#ifndef TODO_CALENDAR_H
#define TODO_CALENDAR_H

#include "todo_model.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 日历查询参数 */
typedef struct {
    int64_t date;           /* 参考日期（Unix 时间戳） */
    int64_t task_system_id; /* -1=全部, 0=默认系统 */
} calendar_query_t;

/* 顺延确认项 */
typedef struct {
    int64_t todo_id;
    int     completed;  /* 0=未完成→顺延 1=已完成 */
} carryover_confirm_t;

/* 顺延报告 */
typedef struct {
    int carried_in_week;
    int overflow_to_next_week;
    int week_promotable;
    int carried_in_month;
    int overflow_to_next_month;
    int month_promotable;
} carryover_report_t;

/* 初始化/关闭 */
void calendar_init(void);
void calendar_shutdown(void);

/* 启动后台定时任务线程 */
void calendar_timer_start(void);

/* 停止后台定时任务线程 */
void calendar_timer_stop(void);

/* 查询某天的所有 todo */
int calendar_day_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count);

/* 查询某周的所有 todo */
int calendar_week_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count);

/* 查询某月的所有 todo */
int calendar_month_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count);

/* 获取待确认的过期任务 */
int calendar_pending_carryover(todo_t **todos, int *count);

/* 批量确认过期任务 */
carryover_report_t calendar_confirm_carryover(carryover_confirm_t *items, int count);

/* 延后某个任务 */
int calendar_postpone(int64_t todo_id, int days);

/* 提升下周/下月任务到本周/本月 */
carryover_report_t calendar_promote(int scope);

#ifdef __cplusplus
}
#endif

#endif /* TODO_CALENDAR_H */
