#ifndef TODO_STATS_H
#define TODO_STATS_H

#include "todo_model.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 周趋势项 */
typedef struct {
    int     week_offset;  /* 0=本周, -1=上周... */
    int64_t week_start;
    int64_t week_end;
    int     completed_count;
} week_trend_t;

/* DFX 统计结果 */
typedef struct {
    int total_completed;
    int on_time_completed;
    int early_completed;
    int carried_over_count;
    int overdue_count;
    double completion_rate;
    int streak_days;
    double avg_early_days;
    double avg_carryover_days;
    int     weekly_trend_count;
    week_trend_t weekly_trend[12]; /* 近 12 周 */
    double plan_completion_rate;
    double temporary_completion_rate;
    int     phase_progress_count;
    double phase_progress[8];    /* 各阶段完成百分比 */
    double plan_health_score;
    double estimation_accuracy;
    double hard_task_ratio;
} dfx_stats_t;

/* 热力图格子 */
typedef struct {
    int64_t date;
    int     completed_count;
    int     level;  /* 0-3 */
} heatmap_cell_t;

/* 热力图周 */
typedef struct {
    int64_t         week_start;
    heatmap_cell_t  days[7];
} heatmap_week_t;

/* 热力图数据 */
typedef struct {
    int             weeks_count;
    heatmap_week_t  weeks[53]; /* 一年最多 53 周 */
    int             max_daily_count;
} heatmap_data_t;

/* 计算 DFX 统计 */
int stats_calculate(dfx_stats_t *stats);

/* 计算热力图 */
int stats_calculate_heatmap(heatmap_data_t *heatmap);

#ifdef __cplusplus
}
#endif

#endif /* TODO_STATS_H */
