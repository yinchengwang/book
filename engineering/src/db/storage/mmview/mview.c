/**
 * @file mview.c
 * @brief 物化视图实现
 */

#include "db/storage/mmview/mview.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========================================================================
 * 全局状态
 * ======================================================================== */

#define MAX_MVIEWS 256
#define MAX_DEPENDENCIES 512
#define MAX_CHANGES 1024

/** 物化视图数组 */
static MViewInfo g_mviews[MAX_MVIEWS];
static int g_mview_count = 0;

/** 依赖关系 */
static MViewDep g_dependencies[MAX_DEPENDENCIES];
static int g_dep_count = 0;

/** 依赖图 */
static MViewGraph g_mview_graph;

/** 变更记录 */
static MViewChange g_changes[MAX_CHANGES];
static int g_change_count = 0;
static uint64_t g_change_id_counter = 0;

/** 刷新调度 */
static MViewSchedule g_schedules[MAX_MVIEWS];

/** 是否初始化 */
static bool g_mview_initialized = false;

/** 物化视图统计 */
static MViewStats g_stats = {0};

/* ========================================================================
 * 初始化
 * ======================================================================== */

/**
 * @brief 初始化物化视图子系统
 */
void mview_init(void)
{
    if (g_mview_initialized) {
        return;
    }

    memset(g_mviews, 0, sizeof(g_mviews));
    memset(g_dependencies, 0, sizeof(g_dependencies));
    memset(g_changes, 0, sizeof(g_changes));
    memset(g_schedules, 0, sizeof(g_schedules));
    memset(&g_mview_graph, 0, sizeof(g_mview_graph));
    memset(&g_stats, 0, sizeof(g_stats));

    g_mview_count = 0;
    g_dep_count = 0;
    g_change_count = 0;
    g_change_id_counter = 0;

    g_mview_initialized = true;
}

/**
 * @brief 关闭物化视图子系统
 */
void mview_shutdown(void)
{
    if (!g_mview_initialized) {
        return;
    }

    /* 清理依赖图 */
    if (g_mview_graph.nodes) {
        for (int i = 0; i < g_mview_graph.node_count; i++) {
            if (g_mview_graph.nodes[i].dependencies) {
                free(g_mview_graph.nodes[i].dependencies);
            }
            if (g_mview_graph.nodes[i].dependents) {
                free(g_mview_graph.nodes[i].dependents);
            }
        }
        free(g_mview_graph.nodes);
    }

    g_mview_initialized = false;
}

/* ========================================================================
 * 物化视图基本操作
 * ======================================================================== */

/**
 * @brief 创建物化视图
 */
int mview_create(const char *name, const char *query,
               MViewRefreshType refresh_type, uint32_t *mview_id)
{
    if (!g_mview_initialized) {
        mview_init();
    }

    if (g_mview_count >= MAX_MVIEWS) {
        return -1;
    }

    uint32_t id = g_mview_count++;
    MViewInfo *mv = &g_mviews[id];

    mv->mview_id = id;
    mv->relfilenode = id + 1000;  /* 简化：分配文件节点 */
    strncpy(mv->name, name ? name : "", sizeof(mv->name) - 1);
    strncpy(mv->query, query ? query : "", sizeof(mv->query) - 1);
    mv->state = MVIEW_STATE_READY;
    mv->refresh_type = refresh_type;
    mv->last_refresh_time = 0;
    mv->last_refresh_duration = 0;
    mv->row_count = 0;
    mv->size_bytes = 0;
    mv->refcount = 0;

    if (mview_id) {
        *mview_id = id;
    }

    g_stats.total_mviews++;

    return 0;
}

/**
 * @brief 删除物化视图
 */
int mview_drop(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return -1;
    }

    MViewInfo *mv = &g_mviews[mview_id];

    /* 清除依赖关系 */
    for (int i = 0; i < g_dep_count; i++) {
        if (g_dependencies[i].mview_id == mview_id ||
            g_dependencies[i].depends_on == mview_id) {
            /* 删除此依赖 */
            memmove(&g_dependencies[i], &g_dependencies[i + 1],
                   (g_dep_count - i - 1) * sizeof(MViewDep));
            g_dep_count--;
            i--;
        }
    }

    mv->state = MVIEW_STATE_INVALID;
    g_stats.total_mviews--;

    return 0;
}

/**
 * @brief 获取物化视图信息
 */
const MViewInfo *mview_get_info(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return NULL;
    }
    return &g_mviews[mview_id];
}

/**
 * @brief 全量刷新物化视图
 */
int mview_refresh_complete(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return -1;
    }

    MViewInfo *mv = &g_mviews[mview_id];
    uint64_t start = 0;  /* TODO: 获取当前时间 */

    mv->state = MVIEW_STATE_REFRESHING;

    /* TODO: 执行全量刷新
     * 1. 执行定义查询
     * 2. 写入物化视图存储
     * 3. 更新统计信息
     */

    mv->state = MVIEW_STATE_READY;
    mv->last_refresh_time = start;
    mv->last_refresh_duration = 0;  /* TODO: 计算耗时 */
    g_stats.total_refreshes++;

    return 0;
}

/**
 * @brief 增量刷新物化视图
 */
int mview_refresh_fast(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return -1;
    }

    MViewInfo *mv = &g_mviews[mview_id];

    if (mv->refresh_type != MVIEW_REFRESH_FAST) {
        return -1;
    }

    uint64_t start = 0;  /* TODO: 获取当前时间 */

    mv->state = MVIEW_STATE_REFRESHING;

    /* TODO: 执行增量刷新
     * 1. 获取相关表的变更记录
     * 2. 应用变更到物化视图
     * 3. 更新统计信息
     */

    mv->state = MVIEW_STATE_READY;
    mv->last_refresh_time = start;
    g_stats.total_refreshes++;

    return 0;
}

/**
 * @brief 刷新所有过期的物化视图
 */
int mview_refresh_stale(bool cascade)
{
    int refreshed = 0;

    for (int i = 0; i < g_mview_count; i++) {
        if (g_mviews[i].state == MVIEW_STATE_STALE) {
            if (cascade) {
                /* 按依赖顺序刷新 */
                uint32_t order[MAX_MVIEWS];
                int count = mview_get_refresh_list(true, order, MAX_MVIEWS);
                for (int j = 0; j < count; j++) {
                    mview_refresh_complete(order[j]);
                    refreshed++;
                }
                break;
            } else {
                mview_refresh_complete(i);
                refreshed++;
            }
        }
    }

    return refreshed;
}

/* ========================================================================
 * 依赖图管理
 * ======================================================================== */

/**
 * @brief 添加物化视图依赖
 */
int mview_add_dependency(uint32_t mview_id, uint32_t depends_on)
{
    if (mview_id >= (uint32_t)g_mview_count ||
        depends_on >= (uint32_t)g_mview_count) {
        return -1;
    }

    if (g_dep_count >= MAX_DEPENDENCIES) {
        return -1;
    }

    /* 检查是否已存在 */
    for (int i = 0; i < g_dep_count; i++) {
        if (g_dependencies[i].mview_id == mview_id &&
            g_dependencies[i].depends_on == depends_on) {
            return 0;
        }
    }

    g_dependencies[g_dep_count].mview_id = mview_id;
    g_dependencies[g_dep_count].depends_on = depends_on;
    g_dependencies[g_dep_count].depends_on_relid = 0;
    g_dep_count++;

    g_mviews[mview_id].refcount++;

    return 0;
}

/**
 * @brief 添加基础表依赖
 */
int mview_add_base_dependency(uint32_t mview_id, uint32_t relfilenode)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return -1;
    }

    if (g_dep_count >= MAX_DEPENDENCIES) {
        return -1;
    }

    g_dependencies[g_dep_count].mview_id = mview_id;
    g_dependencies[g_dep_count].depends_on = 0;
    g_dependencies[g_dep_count].depends_on_relid = relfilenode;
    g_dep_count++;

    return 0;
}

/**
 * @brief 获取物化视图的刷新顺序（拓扑排序）
 */
int mview_get_refresh_order(uint32_t *order, int max_count)
{
    /* Kahn 算法实现拓扑排序 */
    int in_degree[MAX_MVIEWS] = {0};
    int count = 0;

    /* 计算入度 */
    for (int i = 0; i < g_dep_count; i++) {
        if (g_dependencies[i].depends_on > 0 &&
            g_dependencies[i].depends_on < (uint32_t)g_mview_count) {
            in_degree[g_dependencies[i].mview_id]++;
        }
    }

    /* 找到所有入度为 0 的节点 */
    int queue[MAX_MVIEWS];
    int q_head = 0, q_tail = 0;

    for (int i = 0; i < g_mview_count; i++) {
        if (in_degree[i] == 0) {
            queue[q_tail++] = i;
        }
    }

    /* 拓扑排序 */
    while (q_head < q_tail && count < max_count) {
        int node = queue[q_head++];
        order[count++] = node;

        /* 减少依赖节点的入度 */
        for (int i = 0; i < g_dep_count; i++) {
            if (g_dependencies[i].depends_on == (uint32_t)node) {
                int dep_node = g_dependencies[i].mview_id;
                in_degree[dep_node]--;
                if (in_degree[dep_node] == 0) {
                    queue[q_tail++] = dep_node;
                }
            }
        }
    }

    return count;
}

/**
 * @brief 检查是否存在循环依赖
 */
bool mview_has_cycle(void)
{
    int visited[MAX_MVIEWS] = {0};
    int in_stack[MAX_MVIEWS] = {0};

    /* 深度优先搜索检测环 */
    for (int i = 0; i < g_mview_count; i++) {
        if (visited[i] == 0) {
            /* TODO: 实现 DFS 检测 */
        }
    }

    return false;  /* 简化：假设无环 */
}

/**
 * @brief 获取需要刷新的视图列表
 */
int mview_get_refresh_list(bool stale_only, uint32_t *order, int max_count)
{
    int count = mview_get_refresh_order(order, max_count);

    if (stale_only) {
        int write_pos = 0;
        for (int i = 0; i < count && write_pos < max_count; i++) {
            if (g_mviews[order[i]].state == MVIEW_STATE_STALE) {
                order[write_pos++] = order[i];
            }
        }
        count = write_pos;
    }

    return count;
}

/* ========================================================================
 * CDC（变更数据捕获）
 * ======================================================================== */

/**
 * @brief 记录表变更
 */
void mview_record_change(uint32_t relfilenode, uint8_t change_type,
                        const void *change_data, size_t data_size)
{
    if (g_change_count >= MAX_CHANGES) {
        /* 移除最旧的变更 */
        memmove(&g_changes[0], &g_changes[1],
               (MAX_CHANGES - 1) * sizeof(MViewChange));
        g_change_count--;
    }

    MViewChange *ch = &g_changes[g_change_count++];
    ch->relfilenode = relfilenode;
    ch->change_type = change_type;
    ch->change_time = g_change_id_counter++;  /* 简化：使用 ID 作为时间 */
    ch->change_id = ch->change_time;

    if (data_size > sizeof(ch->row_data)) {
        data_size = sizeof(ch->row_data);
    }
    if (change_data && data_size > 0) {
        memcpy(ch->row_data, change_data, data_size);
    }

    /* 标记依赖此表的所有物化视图为过期 */
    for (int i = 0; i < g_dep_count; i++) {
        if (g_dependencies[i].depends_on_relid == relfilenode) {
            uint32_t mv_id = g_dependencies[i].mview_id;
            if (mv_id < (uint32_t)g_mview_count) {
                g_mviews[mv_id].state = MVIEW_STATE_STALE;
                g_stats.stale_mviews++;
            }
        }
    }
}

/**
 * @brief 获取表的未处理变更
 */
int mview_get_changes(uint32_t relfilenode,
                     MViewChange *changes, int max_changes)
{
    int count = 0;

    for (int i = 0; i < g_change_count && count < max_changes; i++) {
        if (g_changes[i].relfilenode == relfilenode) {
            changes[count++] = g_changes[i];
        }
    }

    return count;
}

/**
 * @brief 标记变更已处理
 */
void mview_acknowledge_changes(uint32_t relfilenode, uint64_t change_id)
{
    /* 移除已处理的变更 */
    int write_pos = 0;
    for (int i = 0; i < g_change_count; i++) {
        if (g_changes[i].relfilenode != relfilenode ||
            g_changes[i].change_id > change_id) {
            if (write_pos != i) {
                g_changes[write_pos] = g_changes[i];
            }
            write_pos++;
        }
    }
    g_change_count = write_pos;
}

/**
 * @brief 清除变更记录
 */
void mview_clear_changes(uint32_t relfilenode)
{
    int write_pos = 0;
    for (int i = 0; i < g_change_count; i++) {
        if (g_changes[i].relfilenode != relfilenode) {
            if (write_pos != i) {
                g_changes[write_pos] = g_changes[i];
            }
            write_pos++;
        }
    }
    g_change_count = write_pos;
}

/* ========================================================================
 * 刷新调度
 * ======================================================================== */

/**
 * @brief 设置刷新调度
 */
int mview_set_schedule(uint32_t mview_id, const MViewSchedule *schedule)
{
    if (mview_id >= (uint32_t)g_mview_count || !schedule) {
        return -1;
    }

    g_schedules[mview_id] = *schedule;
    return 0;
}

/**
 * @brief 获取刷新调度
 */
const MViewSchedule *mview_get_schedule(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return NULL;
    }
    return &g_schedules[mview_id];
}

/**
 * @brief 启用刷新调度
 */
int mview_enable_schedule(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return -1;
    }
    g_schedules[mview_id].enabled = true;
    return 0;
}

/**
 * @brief 禁用刷新调度
 */
int mview_disable_schedule(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return -1;
    }
    g_schedules[mview_id].enabled = false;
    return 0;
}

/**
 * @brief 执行定时刷新
 */
int mview_process_scheduled(void)
{
    uint64_t now = 0;  /* TODO: 获取当前时间 */
    int refreshed = 0;

    for (int i = 0; i < g_mview_count; i++) {
        if (g_schedules[i].enabled &&
            g_schedules[i].next_refresh_time <= now &&
            g_mviews[i].state == MVIEW_STATE_STALE) {
            mview_refresh_complete(i);
            g_schedules[i].next_refresh_time = now + g_schedules[i].interval_ms;
            refreshed++;
        }
    }

    return refreshed;
}

/**
 * @brief 获取下一个需要刷新的视图
 */
bool mview_get_next_scheduled(uint32_t *mview_id)
{
    uint64_t now = 0;  /* TODO: 获取当前时间 */
    uint64_t earliest = UINT64_MAX;
    int best = -1;

    for (int i = 0; i < g_mview_count; i++) {
        if (g_schedules[i].enabled &&
            g_schedules[i].next_refresh_time < earliest) {
            earliest = g_schedules[i].next_refresh_time;
            best = i;
        }
    }

    if (best >= 0 && mview_id) {
        *mview_id = best;
        return true;
    }

    return false;
}

/* ========================================================================
 * 查询改写
 * ======================================================================== */

/**
 * @brief 检查查询是否可以改写为使用物化视图
 */
bool mview_can_rewrite(const char *query, uint32_t *mview_id)
{
    if (!query) {
        return false;
    }

    /* 简化实现：检查是否有匹配的物化视图 */
    for (int i = 0; i < g_mview_count; i++) {
        if (g_mviews[i].state == MVIEW_STATE_READY) {
            /* TODO: 实际应该做查询匹配 */
            if (mview_id) {
                *mview_id = i;
            }
            return true;
        }
    }

    return false;
}

/**
 * @brief 获取物化视图的查询替换
 */
int mview_get_rewrite_query(uint32_t mview_id,
                          const char *original_query,
                          char *rewritten_query, size_t max_size)
{
    if (mview_id >= (uint32_t)g_mview_count || !rewritten_query) {
        return -1;
    }

    MViewInfo *mv = &g_mviews[mview_id];

    /* 简化：直接返回 SELECT * FROM mview_name */
    snprintf(rewritten_query, max_size, "SELECT * FROM %s", mv->name);

    return 0;
}

/**
 * @brief 估算使用物化视图的代价
 */
double mview_estimate_cost(uint32_t mview_id)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return 1e10;
    }

    MViewInfo *mv = &g_mviews[mview_id];

    /* 简化：基于大小和年龄估算 */
    double cost = mv->size_bytes / 1024.0;  /* 每 KB 1 单位 */
    cost += mv->last_refresh_duration / 1000.0;  /* 每秒加 1 单位 */

    return cost;
}

/**
 * @brief 检查物化视图是否足够新鲜
 */
bool mview_is_fresh_enough(uint32_t mview_id, uint64_t max_age_ms)
{
    if (mview_id >= (uint32_t)g_mview_count) {
        return false;
    }

    MViewInfo *mv = &g_mviews[mview_id];
    uint64_t now = 0;  /* TODO: 获取当前时间 */

    return (now - mv->last_refresh_time) <= max_age_ms;
}

/* ========================================================================
 * 统计和调试
 * ======================================================================== */

/**
 * @brief 获取物化视图统计
 */
void mview_get_stats(MViewStats *stats)
{
    if (!stats) {
        return;
    }

    *stats = g_stats;
    stats->refreshing_mviews = 0;

    for (int i = 0; i < g_mview_count; i++) {
        if (g_mviews[i].state == MVIEW_STATE_REFRESHING) {
            stats->refreshing_mviews++;
        }
    }
}

/**
 * @brief 打印物化视图状态
 */
void mview_dump(void)
{
    printf("=== Materialized Views ===\n");
    printf("Total: %u, Stale: %u, Refreshing: %u\n",
           g_stats.total_mviews, g_stats.stale_mviews,
           stats.refreshing_mviews);

    for (int i = 0; i < g_mview_count; i++) {
        MViewInfo *mv = &g_mviews[i];
        printf("\n[%u] %s\n", i, mv->name);
        printf("  State: %d\n", mv->state);
        printf("  Rows: %u, Size: %lu bytes\n", mv->row_count, (unsigned long)mv->size_bytes);
        printf("  Last Refresh: %lu, Duration: %lu ms\n",
               (unsigned long)mv->last_refresh_time,
               (unsigned long)mv->last_refresh_duration);
    }
}

/**
 * @brief 打印依赖图
 */
void mview_dump_graph(void)
{
    printf("=== Materialized View Dependencies ===\n");
    printf("Total Dependencies: %d\n", g_dep_count);

    for (int i = 0; i < g_dep_count; i++) {
        printf("  MView %u -> Depends on %u (relid=%u)\n",
               g_dependencies[i].mview_id,
               g_dependencies[i].depends_on,
               g_dependencies[i].depends_on_relid);
    }
}
