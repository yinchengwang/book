/**
 * @file bgworker_config.c
 * @brief 后台任务调度器配置管理
 *
 * 定义并注册后台任务调度器相关的 GUC 配置参数，提供辅助函数
 * 让调度器及其任务可以在运行时读取最新配置。
 */

#include "bgworker_internal.h"
#include "db/guc.h"
#include "db/log.h"
#include <stdbool.h>
#include <stdint.h>

/* ============================================================
 * 配置参数默认值
 * ============================================================ */

/** 是否启用后台任务调度器（默认启用） */
static bool  g_bgworker_enabled = true;

/** 主循环检查间隔（毫秒，默认 1000） */
static int   g_bgworker_tick_ms = 1000;

/** 最大任务槽位数（默认 16） */
static int   g_bgworker_max_tasks = 16;

/** 是否启用 CCEH 缩容任务（默认启用） */
static bool  g_cceh_shrink_enable = true;

/** CCEH 缩容检查间隔（毫秒，默认 5000） */
static int   g_cceh_shrink_interval_ms = 5000;

/** CCEH 缩容负载因子阈值（默认 0.25） */
static double g_cceh_shrink_load_factor_threshold = 0.25;

/** CCEH 缩容时间窗口大小（默认 3） */
static int   g_cceh_shrink_time_window = 3;

/** CCEH 缩容最小全局深度（默认 1） */
static int   g_cceh_shrink_min_global_depth = 1;

/* ============================================================
 * GUC 参数定义
 * ============================================================ */

/**
 * @brief bgworker 模块 GUC 参数注册表
 *
 * 包含调度器全局参数与 CCEH 缩容任务参数。
 */
static guc_param_t g_bgworker_params[] = {
    {
        .name = "bgworker.enabled",
        .type = GUC_TYPE_BOOL,
        .value = &g_bgworker_enabled,
        .reset_val = &g_bgworker_enabled,
        .description = "是否启用后台任务调度器",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.tick_ms",
        .type = GUC_TYPE_INT,
        .value = &g_bgworker_tick_ms,
        .reset_val = &g_bgworker_tick_ms,
        .bounds.int_v = {100, 60000},
        .description = "后台任务调度器主循环检查间隔（毫秒）",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.max_tasks",
        .type = GUC_TYPE_INT,
        .value = &g_bgworker_max_tasks,
        .reset_val = &g_bgworker_max_tasks,
        .bounds.int_v = {1, 256},
        .description = "最大任务槽位数",
        .flags = GUC_FLAG_NO_SHOW
    },
    {
        .name = "bgworker.cceh_shrink.enable",
        .type = GUC_TYPE_BOOL,
        .value = &g_cceh_shrink_enable,
        .reset_val = &g_cceh_shrink_enable,
        .description = "是否启用 CCEH 缩容任务",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.interval_ms",
        .type = GUC_TYPE_INT,
        .value = &g_cceh_shrink_interval_ms,
        .reset_val = &g_cceh_shrink_interval_ms,
        .bounds.int_v = {1000, 3600000},
        .description = "CCEH 缩容检查间隔（毫秒）",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.load_factor_threshold",
        .type = GUC_TYPE_REAL,
        .value = &g_cceh_shrink_load_factor_threshold,
        .reset_val = &g_cceh_shrink_load_factor_threshold,
        .bounds.real_v = {0.05, 0.9},
        .description = "CCEH 缩容负载因子阈值",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.time_window",
        .type = GUC_TYPE_INT,
        .value = &g_cceh_shrink_time_window,
        .reset_val = &g_cceh_shrink_time_window,
        .bounds.int_v = {1, 10},
        .description = "CCEH 缩容时间窗口大小",
        .flags = GUC_FLAG_NONE
    },
    {
        .name = "bgworker.cceh_shrink.min_global_depth",
        .type = GUC_TYPE_INT,
        .value = &g_cceh_shrink_min_global_depth,
        .reset_val = &g_cceh_shrink_min_global_depth,
        .bounds.int_v = {1, 10},
        .description = "CCEH 缩容最小全局深度",
        .flags = GUC_FLAG_NONE
    }
};

/* ============================================================
 * 公共函数
 * ============================================================ */

/**
 * @brief 注册 GUC 配置参数
 *
 * 把 g_bgworker_params 数组中的全部参数注册到 GUC 子系统。
 * 重复注册同一参数名会返回 -1。需要在 GUC 初始化（guc_init）
 * 之后、调度器启动之前调用。
 *
 * @return 0 成功，-1 失败
 */
int db_bgworker_config_init(void) {
    int num_params = (int)(sizeof(g_bgworker_params) / sizeof(g_bgworker_params[0]));

    for (int i = 0; i < num_params; i++) {
        if (guc_register_param(&g_bgworker_params[i]) != 0) {
            LOG_ERROR("注册 GUC 参数 '%s' 失败", g_bgworker_params[i].name);
            return -1;
        }
    }

    LOG_INFO("后台任务调度器配置参数已注册 (%d 个)", num_params);
    return 0;
}

/**
 * @brief 获取 CCEH 缩容任务是否启用
 * @return true 启用，false 禁用
 */
bool db_bgworker_cceh_shrink_is_enabled(void) {
    return g_cceh_shrink_enable;
}

/**
 * @brief 获取 CCEH 缩容任务检查间隔（毫秒）
 * @return 间隔毫秒
 */
int db_bgworker_cceh_shrink_get_interval(void) {
    return g_cceh_shrink_interval_ms;
}
