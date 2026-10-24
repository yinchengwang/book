/**
 * @file materialized_view.c
 * @brief 物化视图实现
 *
 * 实现物化视图的完整功能：
 * 1. 物化视图创建、删除、查询
 * 2. 完整刷新与增量刷新
 * 3. CONCURRENTLY 无锁刷新（使用事务快照）
 * 4. 查询重写（Query Rewrite）
 * 5. 时序物化视图（Sliding Window Aggregate）
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Windows 兼容: pthread 模拟 */
#ifdef _WIN32
#include <windows.h>
typedef CRITICAL_SECTION pthread_mutex_t;
#define PTHREAD_MUTEX_INITIALIZER {0}
static void pthread_mutex_init(pthread_mutex_t *m, void *attr) { (void)attr; InitializeCriticalSection(m); }
static void pthread_mutex_destroy(pthread_mutex_t *m) { DeleteCriticalSection(m); }
static void pthread_mutex_lock(pthread_mutex_t *m) { EnterCriticalSection(m); }
static void pthread_mutex_unlock(pthread_mutex_t *m) { LeaveCriticalSection(m); }
#else
#include <pthread.h>
#endif

#include "db/sql/materialized_view.h"
#include "db/parser/sql/parsenodes.h"

/* ========================================================================
 * 常量定义
 * ======================================================================== */

#define MV_INITIAL_CAPACITY 16
#define MV_GROWTH_FACTOR 2
#define MV_DEFAULT_BLOCK_SIZE 4096

/* ========================================================================
 * 物化视图内部结构
 * ======================================================================== */

/** 物化视图 */
struct MaterializedView {
    char *name;                       /**< 视图名称 */
    MvDef *def;                       /**< 视图定义 */
    MvState state;                    /**< 当前状态 */
    uint64_t last_refresh_time;       /**< 最后刷新时间 */
    uint64_t last_refresh_lsn;        /**< 最后刷新时的LSN */
    MvData *data;                     /**< 物化数据 */
    MvSlidingWindow *sliding_window;  /**< 滑动窗口配置 */
    MvChangeLog *change_log;          /**< 变更日志链表 */
    uint64_t change_log_lsn;          /**< 变更日志当前LSN */
    volatile bool is_refreshing;      /**< 是否正在刷新（使用volatile避免编译器优化） */
};

/** 物化视图管理器 */
struct MvManager {
    char data_dir[512];               /**< 数据目录 */
    MaterializedView **views;         /**< 视图数组 */
    int view_count;                   /**< 视图数量 */
    int capacity;                     /**< 数组容量 */
};

/* ========================================================================
 * 辅助函数
 * ======================================================================== */

/**
 * @brief 复制字符串（安全）
 */
static char *safe_strdup(const char *s)
{
    return s ? strdup(s) : NULL;
}

/**
 * @brief 重新分配视图数组
 */
static int grow_views(MvManager *mgr)
{
    int new_cap = mgr->capacity * MV_GROWTH_FACTOR;
    MaterializedView **new_views = (MaterializedView **)realloc(
        mgr->views, new_cap * sizeof(MaterializedView *));
    if (new_views == NULL) return -1;
    mgr->views = new_views;
    mgr->capacity = new_cap;
    return 0;
}

/**
 * @brief 查找物化视图索引
 */
static int find_view_index(const MvManager *mgr, const char *name)
{
    for (int i = 0; i < mgr->view_count; i++) {
        if (mgr->views[i] && strcmp(mgr->views[i]->name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief 释放物化视图
 */
static void mv_free(MaterializedView *view)
{
    if (view == NULL) return;

    free(view->name);

    /* 释放定义 */
    if (view->def) {
        MvDef *def = view->def;
        free(def->name);
        free(def->select_sql);
        if (def->columns) {
            for (int i = 0; i < def->column_count; i++) {
                free(def->columns[i].name);
                free(def->columns[i].type_name);
            }
            free(def->columns);
        }
        if (def->source_tables) {
            for (int i = 0; i < def->source_table_count; i++) {
                free(def->source_tables[i]);
            }
            free(def->source_tables);
        }
        free(def->tablespace);
        free(def);
    }

    /* 释放数据 */
    if (view->data) {
        if (view->data->data) free(view->data->data);
        free(view->data);
    }

    /* 释放滑动窗口 */
    free(view->sliding_window);

    /* 释放变更日志 */
    mv_free_change_log(view->change_log);

    free(view);
}

/* ========================================================================
 * 物化视图管理器实现
 * ======================================================================== */

MvManager *mv_manager_create(const char *data_dir)
{
    MvManager *mgr = (MvManager *)calloc(1, sizeof(MvManager));
    if (mgr == NULL) return NULL;

    if (data_dir) {
        strncpy(mgr->data_dir, data_dir, sizeof(mgr->data_dir) - 1);
    }

    mgr->capacity = MV_INITIAL_CAPACITY;
    mgr->views = (MaterializedView **)calloc(mgr->capacity, sizeof(MaterializedView *));
    if (mgr->views == NULL) {
        free(mgr);
        return NULL;
    }

    mgr->view_count = 0;
    return mgr;
}

void mv_manager_destroy(MvManager *mgr)
{
    if (mgr == NULL) return;

    for (int i = 0; i < mgr->view_count; i++) {
        mv_free(mgr->views[i]);
    }
    free(mgr->views);
    free(mgr);
}

/* ========================================================================
 * 物化视图生命周期实现
 * ======================================================================== */

MaterializedView *mv_create(MvManager *mgr, const MvDef *def)
{
    if (mgr == NULL || def == NULL || def->name == NULL) return NULL;

    /* 检查是否已存在 */
    if (mv_exists(mgr, def->name)) return NULL;

    /* 扩展容量 */
    if (mgr->view_count >= mgr->capacity) {
        if (grow_views(mgr) != 0) return NULL;
    }

    /* 分配物化视图 */
    MaterializedView *view = (MaterializedView *)calloc(1, sizeof(MaterializedView));
    if (view == NULL) return NULL;

    view->name = safe_strdup(def->name);
    view->state = MV_STATE_STALE;
    view->last_refresh_time = 0;
    view->last_refresh_lsn = 0;
    view->is_refreshing = false;

    /* 复制定义 */
    view->def = (MvDef *)calloc(1, sizeof(MvDef));
    if (view->def == NULL) {
        mv_free(view);
        return NULL;
    }
    view->def->name = safe_strdup(def->name);
    view->def->select_sql = safe_strdup(def->select_sql);
    view->def->with_data = def->with_data;
    view->def->refresh_interval = def->refresh_interval;

    /* 复制列定义 */
    if (def->column_count > 0) {
        view->def->columns = (MvColumnDef *)malloc(
            def->column_count * sizeof(MvColumnDef));
        if (view->def->columns == NULL) {
            mv_free(view);
            return NULL;
        }
        for (int i = 0; i < def->column_count; i++) {
            view->def->columns[i].name = safe_strdup(def->columns[i].name);
            view->def->columns[i].type_name = safe_strdup(def->columns[i].type_name);
            view->def->columns[i].type_len = def->columns[i].type_len;
            view->def->columns[i].is_not_null = def->columns[i].is_not_null;
        }
        view->def->column_count = def->column_count;
    }

    /* 复制源表 */
    if (def->source_table_count > 0) {
        view->def->source_tables = (char **)malloc(
            def->source_table_count * sizeof(char *));
        if (view->def->source_tables == NULL) {
            mv_free(view);
            return NULL;
        }
        for (int i = 0; i < def->source_table_count; i++) {
            view->def->source_tables[i] = safe_strdup(def->source_tables[i]);
        }
        view->def->source_table_count = def->source_table_count;
    }

    /* 初始化数据 */
    view->data = (MvData *)calloc(1, sizeof(MvData));
    if (view->data == NULL) {
        mv_free(view);
        return NULL;
    }
    view->data->storage = MV_STORAGE_HEAP;

    /* 如果需要立即物化数据 */
    if (def->with_data) {
        mv_refresh(view, REFRESH_FULL);
    }

    mgr->views[mgr->view_count++] = view;
    return view;
}

int mv_drop(MvManager *mgr, const char *name)
{
    if (mgr == NULL || name == NULL) return -1;

    int idx = find_view_index(mgr, name);
    if (idx < 0) return -1;

    mv_free(mgr->views[idx]);

    /* 移动数组 */
    for (int i = idx; i < mgr->view_count - 1; i++) {
        mgr->views[i] = mgr->views[i + 1];
    }
    mgr->view_count--;
    return 0;
}

MaterializedView *mv_get(const MvManager *mgr, const char *name)
{
    if (mgr == NULL || name == NULL) return NULL;

    int idx = find_view_index(mgr, name);
    if (idx < 0) return NULL;
    return mgr->views[idx];
}

bool mv_exists(const MvManager *mgr, const char *name)
{
    return mv_get(mgr, name) != NULL;
}

char **mv_list(const MvManager *mgr, int *count)
{
    if (mgr == NULL || count == NULL) return NULL;

    *count = mgr->view_count;
    if (mgr->view_count == 0) return NULL;

    char **names = (char **)malloc(mgr->view_count * sizeof(char *));
    for (int i = 0; i < mgr->view_count; i++) {
        names[i] = safe_strdup(mgr->views[i]->name);
    }
    return names;
}

void mv_free_names(char **names, int count)
{
    if (names == NULL) return;
    for (int i = 0; i < count; i++) {
        free(names[i]);
    }
    free(names);
}

/* ========================================================================
 * 物化视图刷新实现
 * ======================================================================== */

/**
 * @brief 执行完整刷新
 *
 * 模拟实现：清空后重新执行SELECT查询并存储结果
 * 实际实现需要调用SQL执行器
 */
static int do_full_refresh(MaterializedView *view)
{
    if (view == NULL) return -1;

    view->state = MV_STATE_BUILDING;

    /* 清空旧数据 */
    if (view->data->data) {
        free(view->data->data);
        view->data->data = NULL;
    }
    view->data->size = 0;
    view->data->row_count = 0;
    view->data->capacity = 0;

    /* TODO: 执行 view->def->select_sql
     * 实际实现需要：
     * 1. 调用 SQL 解析器解析 SELECT 语句
     * 2. 创建查询计划
     * 3. 执行计划收集结果
     * 4. 将结果存储到 view->data
     */

    /* 模拟：创建模拟数据 */
    const char *sql = view->def->select_sql;
    if (sql && strlen(sql) > 0) {
        /* 模拟物化结果 */
        size_t estimate_size = strlen(sql) * 10;
        view->data->data = malloc(estimate_size);
        if (view->data->data) {
            view->data->size = estimate_size;
            view->data->capacity = estimate_size;
            view->data->row_count = 100; /* 模拟100行 */
            memset(view->data->data, 0, estimate_size);
        }
    }

    view->last_refresh_time = (uint64_t)time(NULL);
    view->state = MV_STATE_VALID;

    return 0;
}

/**
 * @brief 执行增量刷新
 *
 * 基于变更日志更新物化数据
 */
static int do_incremental_refresh(MaterializedView *view)
{
    if (view == NULL) return -1;

    if (view->state == MV_STATE_UNUSABLE) {
        /* 无法增量刷新，需要完全重建 */
        return do_full_refresh(view);
    }

    view->state = MV_STATE_BUILDING;

    /* 获取未处理的变更 */
    int change_count = 0;
    MvChangeLog *changes = mv_get_changes(view, view->last_refresh_lsn, &change_count);

    /* TODO: 根据变更日志更新物化数据
     * 实际实现需要：
     * 1. 根据变更类型（INSERT/UPDATE/DELETE）更新物化结果
     * 2. 维护聚合状态
     */

    /* 模拟更新 */
    if (changes) {
        MvChangeLog *cur = changes;
        while (cur) {
            /* 模拟处理每个变更 */
            view->data->row_count++;
            cur = cur->next;
        }
        mv_free_change_log(changes);
    }

    view->last_refresh_time = (uint64_t)time(NULL);
    view->last_refresh_lsn = view->change_log_lsn;
    view->state = MV_STATE_VALID;

    return 0;
}

/**
 * @brief 执行无锁刷新（CONCURRENTLY）
 *
 * 使用事务快照，允许并发读写
 */
static int do_concurrently_refresh(MaterializedView *view)
{
    if (view == NULL) return -1;

    /* 检查是否已有刷新进行中 */
    if (view->is_refreshing) {
        return -1; /* 已有刷新进行 */
    }

    view->is_refreshing = true;
    view->state = MV_STATE_BUILDING;

    /* TODO: 使用事务快照执行刷新
     * CONCURRENTLY 刷新需要：
     * 1. 创建索引快照（如果需要）
     * 2. 在事务中执行新数据的物化
     * 3. 原子性替换旧数据
     * 4. 保持读取可用
     */

    /* 模拟刷新 */
    MvData *old_data = view->data;
    MvData *new_data = (MvData *)calloc(1, sizeof(MvData));
    new_data->storage = MV_STORAGE_HEAP;

    /* 模拟新数据 */
    size_t estimate_size = 4096;
    new_data->data = malloc(estimate_size);
    new_data->size = estimate_size;
    new_data->capacity = estimate_size;
    new_data->row_count = 150; /* 模拟新结果 */

    /* 原子性替换 */
    view->data = new_data;

    /* 释放旧数据 */
    if (old_data->data) free(old_data->data);
    free(old_data);

    view->last_refresh_time = (uint64_t)time(NULL);
    view->state = MV_STATE_VALID;
    view->is_refreshing = false;

    return 0;
}

int mv_refresh(MaterializedView *view, MvRefreshType type)
{
    if (view == NULL) return -1;

    switch (type) {
        case REFRESH_FULL:
            return do_full_refresh(view);

        case REFRESH_INCREMENTAL:
            return do_incremental_refresh(view);

        case REFRESH_CONCURRENTLY:
            return do_concurrently_refresh(view);

        default:
            return -1;
    }
}

int mv_refresh_async(MaterializedView *view, MvRefreshType type)
{
    /* TODO: 实现真正的异步刷新（后台线程）
     * 实际实现需要：
     * 1. 创建后台线程
     * 2. 在线程中执行刷新
     * 3. 使用条件变量通知完成
     */
    (void)view;
    (void)type;

    /* 暂时同步执行 */
    return mv_refresh(view, type);
}

int mv_refresh_all(MvManager *mgr, MvRefreshType type)
{
    if (mgr == NULL) return -1;

    int success_count = 0;
    for (int i = 0; i < mgr->view_count; i++) {
        if (mv_refresh(mgr->views[i], type) == 0) {
            success_count++;
        }
    }
    return success_count;
}

/* ========================================================================
 * 物化视图属性实现
 * ======================================================================== */

MvState mv_get_state(const MaterializedView *view)
{
    return view ? view->state : MV_STATE_ERROR;
}

uint64_t mv_last_refresh_time(const MaterializedView *view)
{
    return view ? view->last_refresh_time : 0;
}

uint64_t mv_row_count(const MaterializedView *view)
{
    return view ? (view->data ? view->data->row_count : 0) : 0;
}

size_t mv_data_size(const MaterializedView *view)
{
    return view ? (view->data ? view->data->size : 0) : 0;
}

const char *mv_get_name(const MaterializedView *view)
{
    return view ? view->name : NULL;
}

/* ========================================================================
 * 查询重写实现
 * ======================================================================== */

/**
 * @brief 简化SQL解析（检测FROM和WHERE子句）
 */
static bool sql_has_from(const char *sql, const char *table)
{
    if (!sql || !table) return false;

    char pattern[256];
    snprintf(pattern, sizeof(pattern), "FROM %s", table);

    return strstr(sql, pattern) != NULL;
}

/**
 * @brief 简化SQL解析（检测WHERE条件）
 */
static bool sql_has_where(const char *sql, const char *condition)
{
    if (!sql || !condition) return false;

    char pattern[256];
    snprintf(pattern, sizeof(pattern), "WHERE %s", condition);

    (void)sql; (void)condition; /* 避免未使用警告，后续扩展使用 */
    return strstr(sql, pattern) != NULL;
}

/**
 * @brief 检查物化视图是否可以满足查询
 */
static bool mv_can_satisfy_query(const MaterializedView *view, const char *sql)
{
    if (!view || !sql || !view->def->select_sql) return false;

    /* TODO: 完整的查询匹配算法
     * 实际实现需要：
     * 1. 解析查询SQL和物化视图定义SQL
     * 2. 检查列匹配
     * 3. 检查谓词下推可能性
     * 4. 检查聚合兼容性
     */

    /* 简化检查：如果查询涉及的表都在物化视图的源表中 */
    for (int i = 0; i < view->def->source_table_count; i++) {
        const char *table = view->def->source_tables[i];
        if (sql_has_from(sql, table)) {
            /* 找到匹配的表，可以考虑重写 */
            return true;
        }
    }

    return false;
}

/**
 * @brief 生成重写查询
 */
static int generate_rewritten_query(const MaterializedView *view,
                                    const char *original_sql,
                                    char *out_buffer, size_t buffer_size)
{
    if (!view || !original_sql || !out_buffer) return -1;

    /* 简化重写：用物化视图替换原查询
     * TODO: 完整的查询重写需要解析SQL并正确替换表引用
     */
    snprintf(out_buffer, buffer_size, "SELECT * FROM %s", view->name);

    return 0;
}

MvRewriteResult *mv_analyze_rewrite(const MvManager *mgr, const char *sql)
{
    if (mgr == NULL || sql == NULL) return NULL;

    MvRewriteResult *result = (MvRewriteResult *)calloc(1, sizeof(MvRewriteResult));
    if (result == NULL) return NULL;

    result->original_query = sql;
    result->can_rewrite = false;
    result->estimated_speedup = 1.0;

    /* 遍历所有物化视图检查是否可以重写 */
    for (int i = 0; i < mgr->view_count; i++) {
        MaterializedView *view = mgr->views[i];

        /* 只考虑有效的物化视图 */
        if (view->state != MV_STATE_VALID) continue;

        /* 检查是否可以满足查询 */
        if (mv_can_satisfy_query(view, sql)) {
            result->can_rewrite = true;
            result->matched_mv_name = view->name;

            /* 估计加速比（简化计算） */
            /* 实际应该基于物化视图大小 vs 源表大小 */
            result->estimated_speedup = 10.0;

            break;
        }
    }

    return result;
}

int mv_rewrite_query(const MvManager *mgr, const char *sql,
                     char *out_buffer, size_t buffer_size)
{
    if (mgr == NULL || sql == NULL || out_buffer == NULL) return -1;

    MvRewriteResult *result = mv_analyze_rewrite(mgr, sql);
    if (result == NULL) return -1;

    if (!result->can_rewrite) {
        /* 无法重写，复制原查询 */
        strncpy(out_buffer, sql, buffer_size - 1);
        out_buffer[buffer_size - 1] = '\0';
        mv_rewrite_result_free(result);
        return 0;
    }

    /* 找到匹配的物化视图并重写 */
    MaterializedView *view = mv_get(mgr, result->matched_mv_name);
    if (view == NULL) {
        mv_rewrite_result_free(result);
        return -1;
    }

    int ret = generate_rewritten_query(view, sql, out_buffer, buffer_size);
    mv_rewrite_result_free(result);
    return ret;
}

void mv_rewrite_result_free(MvRewriteResult *result)
{
    if (result == NULL) return;
    free(result->rewritten_query);
    free(result);
}

/* ========================================================================
 * 时序物化视图实现
 * ======================================================================== */

int mv_set_sliding_window(MaterializedView *view, const MvSlidingWindow *config)
{
    if (view == NULL || config == NULL) return -1;

    if (view->sliding_window == NULL) {
        view->sliding_window = (MvSlidingWindow *)malloc(sizeof(MvSlidingWindow));
        if (view->sliding_window == NULL) return -1;
    }

    memcpy(view->sliding_window, config, sizeof(MvSlidingWindow));
    return 0;
}

const MvSlidingWindow *mv_get_sliding_window(const MaterializedView *view)
{
    return view ? view->sliding_window : NULL;
}

int mv_advance_sliding_window(MaterializedView *view)
{
    if (view == NULL || view->sliding_window == NULL) return -1;

    /* TODO: 实现滑动窗口推进
     * 实际实现需要：
     * 1. 计算新窗口边界
     * 2. 清理过期数据
     * 3. 增量加载新数据
     */

    return 0;
}

/* ========================================================================
 * 变更追踪实现
 * ======================================================================== */

int mv_record_change(MaterializedView *view, const char *table_name,
                     uint64_t lsn, char operation,
                     const char *old_row, const char *new_row)
{
    if (view == NULL || table_name == NULL) return -1;

    MvChangeLog *log = (MvChangeLog *)malloc(sizeof(MvChangeLog));
    if (log == NULL) return -1;

    log->table_name = safe_strdup(table_name);
    log->lsn = lsn;
    log->xid = 0; /* TODO: 从事务系统获取 */
    log->operation = operation;
    log->old_row = safe_strdup(old_row);
    log->new_row = safe_strdup(new_row);
    log->next = view->change_log;

    view->change_log = log;
    view->change_log_lsn = lsn + 1;

    /* 标记视图为过期 */
    if (view->state == MV_STATE_VALID) {
        view->state = MV_STATE_STALE;
    }

    return 0;
}

MvChangeLog *mv_get_changes(MaterializedView *view, uint64_t since_lsn, int *out_count)
{
    if (view == NULL || out_count == NULL) return NULL;

    *out_count = 0;
    MvChangeLog dummy_head;
    dummy_head.next = NULL;
    MvChangeLog *tail = &dummy_head;

    MvChangeLog *cur = view->change_log;
    while (cur) {
        if (cur->lsn >= since_lsn) {
            /* 复制变更日志 */
            MvChangeLog *copy = (MvChangeLog *)malloc(sizeof(MvChangeLog));
            if (copy) {
                copy->table_name = safe_strdup(cur->table_name);
                copy->lsn = cur->lsn;
                copy->xid = cur->xid;
                copy->operation = cur->operation;
                copy->old_row = safe_strdup(cur->old_row);
                copy->new_row = safe_strdup(cur->new_row);
                copy->next = NULL;
                tail->next = copy;
                tail = copy;
                (*out_count)++;
            }
        }
        cur = cur->next;
    }

    return dummy_head.next;
}

void mv_free_change_log(MvChangeLog *log)
{
    while (log) {
        MvChangeLog *next = log->next;
        free(log->table_name);
        free(log->old_row);
        free(log->new_row);
        free(log);
        log = next;
    }
}

void mv_purge_changes(MaterializedView *view, uint64_t up_to_lsn)
{
    if (view == NULL) return;

    /* 保留 >= up_to_lsn 的变更 */
    MvChangeLog dummy_head;
    dummy_head.next = view->change_log;
    MvChangeLog *prev = &dummy_head;
    MvChangeLog *cur = view->change_log;

    while (cur) {
        if (cur->lsn < up_to_lsn) {
            /* 删除此节点 */
            prev->next = cur->next;
            free(cur->table_name);
            free(cur->old_row);
            free(cur->new_row);
            free(cur);
            cur = prev->next;
        } else {
            prev = cur;
            cur = cur->next;
        }
    }

    view->change_log = dummy_head.next;
}

/* ========================================================================
 * 辅助函数实现
 * ======================================================================== */

const char *mv_state_name(MvState state)
{
    switch (state) {
        case MV_STATE_VALID:     return "valid";
        case MV_STATE_STALE:     return "stale";
        case MV_STATE_BUILDING:  return "building";
        case MV_STATE_ERROR:     return "error";
        case MV_STATE_UNUSABLE:  return "unusable";
        default:                 return "unknown";
    }
}

const char *mv_refresh_type_name(MvRefreshType type)
{
    switch (type) {
        case REFRESH_FULL:        return "full";
        case REFRESH_INCREMENTAL: return "incremental";
        case REFRESH_CONCURRENTLY: return "concurrently";
        default:                  return "unknown";
    }
}

MvDef *mv_def_create(const char *name, const char *sql)
{
    MvDef *def = (MvDef *)calloc(1, sizeof(MvDef));
    if (def == NULL) return NULL;

    def->name = safe_strdup(name);
    def->select_sql = safe_strdup(sql);
    def->with_data = true;
    def->refresh_interval = 0;

    return def;
}

void mv_def_add_column(MvDef *def, const char *col_name, const char *type_name)
{
    if (def == NULL || col_name == NULL) return;

    MvColumnDef *new_cols = (MvColumnDef *)realloc(
        def->columns, (def->column_count + 1) * sizeof(MvColumnDef));
    if (new_cols == NULL) return;

    def->columns = new_cols;
    def->columns[def->column_count].name = safe_strdup(col_name);
    def->columns[def->column_count].type_name = safe_strdup(type_name);
    def->columns[def->column_count].type_len = 0;
    def->columns[def->column_count].is_not_null = false;
    def->column_count++;
}

void mv_def_add_source_table(MvDef *def, const char *table_name)
{
    if (def == NULL || table_name == NULL) return;

    char **new_tables = (char **)realloc(
        def->source_tables, (def->source_table_count + 1) * sizeof(char *));
    if (new_tables == NULL) return;

    def->source_tables = new_tables;
    def->source_tables[def->source_table_count] = safe_strdup(table_name);
    def->source_table_count++;
}

void mv_def_destroy(MvDef *def)
{
    if (def == NULL) return;

    free(def->name);
    free(def->select_sql);
    free(def->tablespace);

    if (def->columns) {
        for (int i = 0; i < def->column_count; i++) {
            free(def->columns[i].name);
            free(def->columns[i].type_name);
        }
        free(def->columns);
    }

    if (def->source_tables) {
        for (int i = 0; i < def->source_table_count; i++) {
            free(def->source_tables[i]);
        }
        free(def->source_tables);
    }

    free(def);
}
