#include "todo_stats.h"
#include "todo_model.h"
#include "todo_db.h"
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sqlite3.h>

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
    if (wday == 0) wday = 7;
    return ds - (int64_t)(wday - 1) * 86400;
}

/* ============================================================
 * stats_calculate - 计算 DFX 统计（基于 SQLite 查询）
 * ============================================================ */
int stats_calculate(dfx_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;
    int64_t today = today_start();
    sqlite3_stmt *stmt = NULL;

    /* 总完成数、按时完成数、提前完成数、顺延数 */
    if (sqlite3_prepare_v2(db,
        "SELECT COUNT(*),"
        "  SUM(CASE WHEN completed_at > 0 AND due_date > 0 AND completed_at <= due_date THEN 1 ELSE 0 END),"
        "  SUM(CASE WHEN completed_at > 0 AND due_date > 0 AND completed_at < due_date - 43200 THEN 1 ELSE 0 END),"
        "  SUM(CASE WHEN carryover_count > 0 THEN 1 ELSE 0 END),"
        "  SUM(CASE WHEN plan_id > 0 THEN 1 ELSE 0 END),"
        "  SUM(CASE WHEN plan_id = 0 THEN 1 ELSE 0 END)"
        " FROM todos WHERE status='closed'",
        -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
        stats->total_completed = sqlite3_column_int(stmt, 0);
        stats->on_time_completed = sqlite3_column_int(stmt, 1);
        stats->early_completed = sqlite3_column_int(stmt, 2);
        stats->carried_over_count = sqlite3_column_int(stmt, 3);
        stats->plan_completion_rate = 0;
        stats->temporary_completion_rate = 0;
    }
    sqlite3_finalize(stmt);

    /* 提前完成平均天数 */
    if (stats->early_completed > 0) {
        if (sqlite3_prepare_v2(db,
            "SELECT AVG((due_date - completed_at) / 86400.0) FROM todos "
            "WHERE status='closed' AND completed_at > 0 AND due_date > 0 AND completed_at < due_date - 43200",
            -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            stats->avg_early_days = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* 平均顺延次数 */
    if (stats->carried_over_count > 0) {
        if (sqlite3_prepare_v2(db,
            "SELECT AVG(carryover_count) FROM todos WHERE status='closed' AND carryover_count > 0",
            -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            stats->avg_carryover_days = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    /* 困难任务占比（顺延 >= 2 次） */
    if (stats->total_completed > 0) {
        if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM todos WHERE status='closed' AND carryover_count >= 2",
            -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW) {
            stats->hard_task_ratio = (double)sqlite3_column_int(stmt, 0) / stats->total_completed;
        }
        sqlite3_finalize(stmt);
    }

    /* 逾期未完成 */
    if (sqlite3_prepare_v2(db,
        "SELECT COUNT(*) FROM todos WHERE status='open' AND due_date > 0 AND due_date < ?",
        -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, today);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            stats->overdue_count = sqlite3_column_int(stmt, 0);
        }
    }
    sqlite3_finalize(stmt);

    /* 完成率 */
    int total_active = stats->total_completed + stats->overdue_count;
    stats->completion_rate = total_active > 0
        ? (double)stats->total_completed / total_active : 0.0;

    /* 连续完成天数（从昨天往前推） */
    stats->streak_days = 0;
    int64_t check_date = today - 86400;
    for (int d = 0; d < 365; d++) {
        if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM todos WHERE status='closed' AND completed_at >= ? AND completed_at < ?",
            -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, check_date);
            sqlite3_bind_int64(stmt, 2, check_date + 86400);
            int found = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_int(stmt, 0) > 0)
                found = 1;
            sqlite3_finalize(stmt);
            if (!found) break;
            stats->streak_days++;
            check_date -= 86400;
        } else {
            break;
        }
    }

    /* 周趋势（近 12 周） */
    stats->weekly_trend_count = 0;
    for (int w = 0; w < 12; w++) {
        int64_t ws = week_start(today) - (int64_t)w * 7 * 86400;
        int64_t we = ws + 7 * 86400;
        if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM todos WHERE status='closed' AND completed_at >= ? AND completed_at < ?",
            -1, &stmt, NULL) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, ws);
            sqlite3_bind_int64(stmt, 2, we);
            int wc = 0;
            if (sqlite3_step(stmt) == SQLITE_ROW)
                wc = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
            stats->weekly_trend[stats->weekly_trend_count].week_offset = -w;
            stats->weekly_trend[stats->weekly_trend_count].week_start = ws;
            stats->weekly_trend[stats->weekly_trend_count].week_end = we;
            stats->weekly_trend[stats->weekly_trend_count].completed_count = wc;
            stats->weekly_trend_count++;
        }
    }

    /* 计划 vs 临时任务完成率 */
    {
        int plan_completed = 0, temp_completed = 0, total_plan = 0, total_temp = 0;
        if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM todos WHERE status='closed' AND plan_id > 0",
            -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
            plan_completed = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM todos WHERE status='closed' AND plan_id = 0",
            -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
            temp_completed = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM todos WHERE (status='closed' OR status='open') AND plan_id > 0",
            -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
            total_plan = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (sqlite3_prepare_v2(db,
            "SELECT COUNT(*) FROM todos WHERE (status='closed' OR status='open') AND plan_id = 0",
            -1, &stmt, NULL) == SQLITE_OK && sqlite3_step(stmt) == SQLITE_ROW)
            total_temp = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (total_plan > 0)
            stats->plan_completion_rate = (double)plan_completed / total_plan;
        if (total_temp > 0)
            stats->temporary_completion_rate = (double)temp_completed / total_temp;
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

    stats->estimation_accuracy = 0.0;
    stats->phase_progress_count = 0;

    return 0;
}

/* ============================================================
 * stats_calculate_heatmap - 计算热力图（基于 SQLite）
 * ============================================================ */
int stats_calculate_heatmap(heatmap_data_t *heatmap) {
    memset(heatmap, 0, sizeof(*heatmap));
    sqlite3 *db = todo_db_handle();
    if (!db) return -1;
    int64_t today = today_start();

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    struct tm past = *lt;
    past.tm_year -= 1;
    (void)mktime(&past);
    int64_t one_year_ago = (int64_t)mktime(&past);

    int64_t ws = week_start(one_year_ago);
    int max_count = 0;
    heatmap->weeks_count = 0;
    sqlite3_stmt *stmt = NULL;

    while (ws < today && heatmap->weeks_count < 53) {
        heatmap_week_t *week = &heatmap->weeks[heatmap->weeks_count];
        week->week_start = ws;

        for (int d = 0; d < 7; d++) {
            int64_t day_ts = ws + (int64_t)d * 86400;
            int count = 0;

            if (sqlite3_prepare_v2(db,
                "SELECT COUNT(*) FROM todos WHERE status='closed' AND completed_at >= ? AND completed_at < ?",
                -1, &stmt, NULL) == SQLITE_OK) {
                sqlite3_bind_int64(stmt, 1, day_ts);
                sqlite3_bind_int64(stmt, 2, day_ts + 86400);
                if (sqlite3_step(stmt) == SQLITE_ROW)
                    count = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
            }

            heatmap_cell_t *cell = &week->days[d];
            cell->date = day_ts;
            cell->completed_count = count;
            if (count > max_count) max_count = count;
        }

        heatmap->weeks_count++;
        ws += 7 * 86400;
    }

    heatmap->max_daily_count = max_count;
    int threshold = max_count > 0 ? max_count / 3 : 1;
    if (threshold < 1) threshold = 1;

    for (int w = 0; w < heatmap->weeks_count; w++) {
        for (int d = 0; d < 7; d++) {
            int c = heatmap->weeks[w].days[d].completed_count;
            if (c == 0) heatmap->weeks[w].days[d].level = 0;
            else if (c <= threshold) heatmap->weeks[w].days[d].level = 1;
            else if (c <= threshold * 2) heatmap->weeks[w].days[d].level = 2;
            else heatmap->weeks[w].days[d].level = 3;
        }
    }

    return 0;
}