/**
 * @file fault_inject.c
 * @brief 故障注入框架实现
 *
 * 使用静态规则数组，支持最多 FAULT_MAX_RULES（16）条规则。
 * fault_should_fail() 遍历规则数组，O(n) 复杂度（n ≤ 16）。
 */
#include "db/fault_inject.h"
#include <string.h>

/* ============================================================
 * 内部状态
 * ============================================================ */

/** 故障注入规则数组 */
static fault_config_t g_fault_rules[FAULT_MAX_RULES];

/** 活跃规则数 */
static int g_num_rules = 0;

/** 总触发次数 */
static uint32_t g_total_triggers = 0;

/** 是否已初始化 */
static bool g_initialized = false;

/* ============================================================
 * API 实现
 * ============================================================ */

void fault_init(void) {
    memset(g_fault_rules, 0, sizeof(g_fault_rules));
    g_num_rules = 0;
    g_total_triggers = 0;
    g_initialized = true;
}

int fault_register(const fault_config_t *config) {
    if (!g_initialized) {
        fault_init();
    }

    if (!config || g_num_rules >= FAULT_MAX_RULES) {
        return -1;
    }

    g_fault_rules[g_num_rules] = *config;
    g_fault_rules[g_num_rules].hits = 0;
    g_num_rules++;

    return 0;
}

void fault_clear(void) {
    memset(g_fault_rules, 0, sizeof(g_fault_rules));
    g_num_rules = 0;
    g_total_triggers = 0;
}

bool fault_should_fail(fault_point_t point) {
    if (!g_initialized || g_num_rules == 0) {
        return false;
    }

    for (int i = 0; i < g_num_rules; i++) {
        fault_config_t *rule = &g_fault_rules[i];

        if (rule->point != point) {
            continue;
        }

        /* 跳过初始化阶段的 N 次调用 */
        if (rule->hits < rule->skip_count) {
            rule->hits++;
            continue;
        }

        /* 触发故障 */
        rule->hits++;
        g_total_triggers++;

        /* 检查是否达到最大触发次数 */
        if (rule->max_hits > 0 && rule->hits - rule->skip_count >= rule->max_hits) {
            /* 移除该规则（与最后一个规则交换） */
            g_fault_rules[i] = g_fault_rules[g_num_rules - 1];
            memset(&g_fault_rules[g_num_rules - 1], 0, sizeof(fault_config_t));
            g_num_rules--;
        }

        return true;
    }

    return false;
}

void fault_get_stats(fault_stats_t *stats) {
    if (!stats) return;

    stats->total_triggers = g_total_triggers;
    stats->active_rules = g_num_rules;
    stats->exhausted_rules = 0;
}