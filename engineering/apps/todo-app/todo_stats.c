#include "todo_stats.h"
#include "todo_model.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

/* ============================================================
 * 辅助函数：时间计算
 * ============================================================ */

/* 获取今天的 Unix 时间戳（00:00:00 UTC） */
static int64_t today_start(void) {
    time_t now = time(NULL);
    return (int64_t)(now / 86400) * 86400;
}

/* 获取某天的 00:00:00 */
static int64_t day_start(int64_t ts) {
    return (ts / 86400) * 86400;
}

/* 获取某周的周一 00:00:00 */
static int64_t week_start(int64_t ts) {
    int64_t ds = day_start(ts);
    time_t t = (time_t)ds;
    struct tm *lt = localtime(&t);
    int wday = lt->tm_wday;
    if (wday == 0) wday = 7;  /* 周日转为 7 */
    return ds - (int64_t)(wday - 1) * 86400;
}

/* ============================================================
 * stats_calculate - 计算 DFX 统计
 * ============================================================ */
int stats_calculate(dfx_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    int64_t today = today_start();

    int has_plan_tasks = 0;
    int has_temporary_tasks = 0;
    int plan_completed = 0;
    int temporary_completed = 0;

    /* 遍历所有 closed 的 todo */
    for (int i = 0; i < g_todo_count; i++) {
        todo_t *t = &g_todos[i];
        if (strcmp(t->status, "closed") != 0) continue;
        stats->total_completed++;

        /* 按时/提前完成判定 */
        if (t->completed_at > 0 && t->due_date > 0) {
            if (t->completed_at <= t->due_date) {
                stats->on_time_completed++;
                /* 提前超过半天才算提前完成 */
                if (t->completed_at < t->due_date - 43200) {
                    stats->early_completed++;
                    stats->avg_early_days += (double)(t->due_date - t->completed_at) / 86400.0;
                }
            }
        }

        /* 顺延统计 */
        if (t->carryover_count > 0) {
            stats->carried_over_count++;
            stats->avg_carryover_days += (double)t->carryover_count;
            if (t->carryover_count >= 2) stats->hard_task_ratio += 1.0;
        }

        /* 计划 vs 临时任务 */
        if (t->plan_id > 0) {
            has_plan_tasks++;
            plan_completed++;
        } else {
            has_temporary_tasks++;
            temporary_completed++;
        }
    }

    /* 平均值 */
    if (stats->early_completed > 0)
        stats->avg_early_days /= stats->early_completed;
    if (stats->carried_over_count > 0)
        stats->avg_carryover_days /= stats->carried_over_count;

    /* 逾期未完成 */
    stats->overdue_count = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") == 0 &&
            g_todos[i].due_date > 0 && g_todos[i].due_date < today) {
            stats->overdue_count++;
        }
    }

    /* 完成率 */
    int total_active = stats->total_completed + stats->overdue_count;
    stats->completion_rate = total_active > 0
        ? (double)stats->total_completed / total_active : 0.0;

    /* 连续完成天数（从昨天往前推） */
    stats->streak_days = 0;
    int64_t check_date = today - 86400;
    int max_streak_check = 365;
    while (max_streak_check > 0) {
        int found = 0;
        for (int i = 0; i < g_todo_count; i++) {
            if (strcmp(g_todos[i].status, "closed") == 0 &&
                g_todos[i].completed_at >= check_date &&
                g_todos[i].completed_at < check_date + 86400) {
                found = 1;
                break;
            }
        }
        if (!found) break;
        stats->streak_days++;
        check_date -= 86400;
        max_streak_check--;
    }

    /* 周趋势（近 12 周） */
    stats->weekly_trend_count = 0;
    for (int w = 0; w < 12; w++) {
        int64_t ws = week_start(today) - (int64_t)w * 7 * 86400;
        int64_t we = ws + 7 * 86400;
        int wc = 0;
        for (int i = 0; i < g_todo_count; i++) {
            if (strcmp(g_todos[i].status, "closed") == 0 &&
                g_todos[i].completed_at >= ws &&
                g_todos[i].completed_at < we) {
                wc++;
            }
        }
        stats->weekly_trend[stats->weekly_trend_count].week_offset = -w;
        stats->weekly_trend[stats->weekly_trend_count].week_start = ws;
        stats->weekly_trend[stats->weekly_trend_count].week_end = we;
        stats->weekly_trend[stats->weekly_trend_count].completed_count = wc;
        stats->weekly_trend_count++;
    }

    /* 计划 vs 临时任务完成率 */
    stats->plan_completion_rate = 0.0;
    stats->temporary_completion_rate = 0.0;
    {
        int total_plan = 0, total_temp = 0;
        for (int i = 0; i < g_todo_count; i++) {
            if (strcmp(g_todos[i].status, "closed") == 0 ||
                strcmp(g_todos[i].status, "open") == 0) {
                if (g_todos[i].plan_id > 0) total_plan++;
                else total_temp++;
            }
        }
        if (total_plan > 0)
            stats->plan_completion_rate = (double)plan_completed / total_plan;
        if (total_temp > 0)
            stats->temporary_completion_rate = (double)temporary_completed / total_temp;
    }

    /* 计划健康度 */
    {
        double completion_weight = 0.5;
        double streak_weight = 0.2;
        double carryover_penalty = 0.0;

        if (stats->total_completed > 0 && stats->carried_over_count > 0) {
            carryover_penalty = 0.3 * (1.0 - (double)stats->carried_over_count
                / (double)stats->total_completed);
        }

        stats->plan_health_score = (stats->completion_rate * completion_weight
            + (double)stats->streak_days / 30.0 * streak_weight
            + carryover_penalty) * 100.0;

        if (stats->plan_health_score > 100.0) stats->plan_health_score = 100.0;
        if (stats->plan_health_score < 0.0)   stats->plan_health_score = 0.0;
    }

    /* 困难任务占比 */
    if (stats->total_completed > 0 && stats->hard_task_ratio > 0) {
        stats->hard_task_ratio /= stats->total_completed;
    }

    /* estimation_accuracy 暂设为 0（需要预估时间数据支持） */
    stats->estimation_accuracy = 0.0;

    /* phase_progress 暂不实现（需要阶段数据） */
    stats->phase_progress_count = 0;

    return 0;
}

/* ============================================================
 * stats_calculate_heatmap - 计算热力图
 * ============================================================ */
int stats_calculate_heatmap(heatmap_data_t *heatmap) {
    memset(heatmap, 0, sizeof(*heatmap));
    int64_t today = today_start();

    /* 计算一年前的日期 */
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    struct tm past = *lt;
    past.tm_year -= 1;
    (void)mktime(&past);  /* 规范化 */
    int64_t one_year_ago = (int64_t)mktime(&past);

    /* 从一年前开始，按周分组 */
    int64_t ws = week_start(one_year_ago);
    int max_count = 0;
    heatmap->weeks_count = 0;

    while (ws < today && heatmap->weeks_count < 53) {
        heatmap_week_t *week = &heatmap->weeks[heatmap->weeks_count];
        week->week_start = ws;

        for (int d = 0; d < 7; d++) {
            int64_t day_ts = ws + (int64_t)d * 86400;
            int count = 0;

            for (int i = 0; i < g_todo_count; i++) {
                if (strcmp(g_todos[i].status, "closed") == 0 &&
                    g_todos[i].completed_at >= day_ts &&
                    g_todos[i].completed_at < day_ts + 86400) {
                    count++;
                }
            }

            heatmap_cell_t *cell = &week->days[d];
            cell->date = day_ts;
            cell->completed_count = count;
            if (count > max_count) max_count = count;
        }

        heatmap->weeks_count++;
        ws += 7 * 86400;
    }

    /* 计算色级（0-3） */
    heatmap->max_daily_count = max_count;
    int threshold = max_count > 0 ? max_count / 3 : 1;
    if (threshold < 1) threshold = 1;

    for (int w = 0; w < heatmap->weeks_count; w++) {
        for (int d = 0; d < 7; d++) {
            int c = heatmap->weeks[w].days[d].completed_count;
            if (c == 0) {
                heatmap->weeks[w].days[d].level = 0;
            } else if (c <= threshold) {
                heatmap->weeks[w].days[d].level = 1;
            } else if (c <= threshold * 2) {
                heatmap->weeks[w].days[d].level = 2;
            } else {
                heatmap->weeks[w].days[d].level = 3;
            }
        }
    }

    return 0;
}