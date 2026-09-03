/*
 * view.c - 视图实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#include <db/view/view.h>

/* ─────────────────────────────────────────────────────────────────
 * 视图结构
 * ───────────────────────────────────────────────────────────────── */

struct view {
    char *name;                     /* 视图名称 */
    view_type_t type;              /* 视图类型 */
    char *select_sql;            /* SELECT 查询 */
    char **column_names;           /* 列名 */
    int column_count;              /* 列数 */
    view_refresh_t refresh;        /* 刷新策略 */
    int refresh_interval;          /* 刷新间隔 */
    bool with_check_option;        /* WITH CHECK OPTION */

    /* 物化视图特有 */
    mv_state_t state;              /* 状态 */
    uint64_t last_refresh;         /* 最后刷新时间 */
    void *data;                    /* 物化数据（简化实现） */
    size_t data_size;              /* 数据大小 */
};

/* ─────────────────────────────────────────────────────────────────
 * 视图管理器结构
 * ───────────────────────────────────────────────────────────────── */

struct view_manager {
    view_t **views;        /* 视图数组 */
    int view_count;       /* 视图数量 */
    int capacity;         /* 数组容量 */
};

/* ─────────────────────────────────────────────────────────────────
 * 视图管理器实现
 * ───────────────────────────────────────────────────────────────── */

view_manager_t *view_manager_create(void)
{
    view_manager_t *mgr = (view_manager_t *)calloc(1, sizeof(view_manager_t));
    if (mgr == NULL) return NULL;

    mgr->capacity = 16;
    mgr->views = (view_t **)calloc(mgr->capacity, sizeof(view_t *));
    if (mgr->views == NULL) {
        free(mgr);
        return NULL;
    }

    mgr->view_count = 0;
    return mgr;
}

void view_manager_destroy(view_manager_t *mgr)
{
    if (mgr == NULL) return;

    for (int i = 0; i < mgr->view_count; i++) {
        if (mgr->views[i]) {
            /* view_destroy 会释放内存 */
            free(mgr->views[i]->name);
            if (mgr->views[i]->select_sql) free(mgr->views[i]->select_sql);
            if (mgr->views[i]->column_names) {
                for (int j = 0; j < mgr->views[i]->column_count; j++) {
                    if (mgr->views[i]->column_names[j]) free(mgr->views[i]->column_names[j]);
                }
                free(mgr->views[i]->column_names);
            }
            if (mgr->views[i]->data) free(mgr->views[i]->data);
            free(mgr->views[i]);
        }
    }
    free(mgr->views);
    free(mgr);
}

view_t *view_create(view_manager_t *mgr, const view_def_t *def)
{
    if (mgr == NULL || def == NULL) return NULL;

    /* 检查是否已存在 */
    if (view_exists(mgr, def->name)) return NULL;

    /* 扩展容量 */
    if (mgr->view_count >= mgr->capacity) {
        int new_cap = mgr->capacity * 2;
        view_t **new_views = (view_t **)realloc(mgr->views, new_cap * sizeof(view_t *));
        if (new_views == NULL) return NULL;
        mgr->views = new_views;
        mgr->capacity = new_cap;
    }

    view_t *view = (view_t *)calloc(1, sizeof(view_t));
    if (view == NULL) return NULL;

    view->name = strdup(def->name);
    view->type = def->type;
    view->select_sql = def->select_sql ? strdup(def->select_sql) : NULL;
    view->column_count = def->column_count;
    view->refresh = def->refresh;
    view->refresh_interval = def->refresh_interval;
    view->with_check_option = def->with_check_option;

    /* 复制列名 */
    if (def->column_names && def->column_count > 0) {
        view->column_names = (char **)malloc(def->column_count * sizeof(char *));
        for (int i = 0; i < def->column_count; i++) {
            view->column_names[i] = def->column_names[i] ?
                strdup(def->column_names[i]) : NULL;
        }
    }

    /* 物化视图初始化 */
    if (def->type == VIEW_MATERIALIZED) {
        view->state = MV_STATE_STALE;  /* 初始为过期状态 */
        view->data = NULL;
        view->data_size = 0;
    } else {
        view->state = MV_STATE_VALID;
    }

    view->last_refresh = 0;

    mgr->views[mgr->view_count++] = view;
    return view;
}

int view_drop(view_manager_t *mgr, const char *name)
{
    if (mgr == NULL || name == NULL) return -1;

    for (int i = 0; i < mgr->view_count; i++) {
        if (mgr->views[i] && strcmp(mgr->views[i]->name, name) == 0) {
            /* 释放视图内存 */
            free(mgr->views[i]->name);
            if (mgr->views[i]->select_sql) free(mgr->views[i]->select_sql);
            if (mgr->views[i]->column_names) {
                for (int j = 0; j < mgr->views[i]->column_count; j++) {
                    if (mgr->views[i]->column_names[j]) free(mgr->views[i]->column_names[j]);
                }
                free(mgr->views[i]->column_names);
            }
            if (mgr->views[i]->data) free(mgr->views[i]->data);
            free(mgr->views[i]);

            /* 移动数组 */
            for (int j = i; j < mgr->view_count - 1; j++) {
                mgr->views[j] = mgr->views[j + 1];
            }
            mgr->view_count--;
            return 0;
        }
    }
    return -1;
}

view_t *view_get(view_manager_t *mgr, const char *name)
{
    if (mgr == NULL || name == NULL) return NULL;

    for (int i = 0; i < mgr->view_count; i++) {
        if (mgr->views[i] && strcmp(mgr->views[i]->name, name) == 0) {
            return mgr->views[i];
        }
    }
    return NULL;
}

bool view_exists(const view_manager_t *mgr, const char *name)
{
    return view_get((view_manager_t *)mgr, name) != NULL;
}

char **view_list(const view_manager_t *mgr, int *count)
{
    if (mgr == NULL || count == NULL) return NULL;

    *count = mgr->view_count;
    if (mgr->view_count == 0) return NULL;

    char **names = (char **)malloc(mgr->view_count * sizeof(char *));
    for (int i = 0; i < mgr->view_count; i++) {
        names[i] = mgr->views[i] ? strdup(mgr->views[i]->name) : NULL;
    }
    return names;
}

/* ─────────────────────────────────────────────────────────────────
 * 物化视图操作实现
 * ───────────────────────────────────────────────────────────────── */

int view_refresh(view_t *view)
{
    if (view == NULL) return -1;

    if (view->type != VIEW_MATERIALIZED) {
        /* 普通视图不需要刷新 */
        return 0;
    }

    /* TODO: 执行 SELECT 查询并存储结果
     * 1. 解析 view->select_sql
     * 2. 执行查询
     * 3. 存储结果到 view->data
     */

    view->state = MV_STATE_BUILDING;

    /* 模拟刷新 */
    view->data = realloc(view->data, 1024);
    view->data_size = 1024;
    view->last_refresh = (uint64_t)time(NULL);
    view->state = MV_STATE_VALID;

    return 0;
}

int view_refresh_async(view_t *view)
{
    if (view == NULL) return -1;

    /* TODO: 实现异步刷新（后台线程） */
    return view_refresh(view);
}

mv_state_t view_get_state(const view_t *view)
{
    return view ? view->state : MV_STATE_ERROR;
}

uint64_t view_last_refresh(const view_t *view)
{
    return view ? view->last_refresh : 0;
}

/* ─────────────────────────────────────────────────────────────────
 * 视图定义操作实现
 * ───────────────────────────────────────────────────────────────── */

const view_def_t *view_get_def(const view_t *view)
{
    /* 简化实现：返回 NULL，实际应返回内部定义 */
    (void)view;
    return NULL;
}

view_type_t view_get_type(const view_t *view)
{
    return view ? view->type : VIEW_REGULAR;
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数实现
 * ───────────────────────────────────────────────────────────────── */

view_def_t *view_def_create(const char *name, const char *sql, view_type_t type)
{
    view_def_t *def = (view_def_t *)calloc(1, sizeof(view_def_t));
    if (def == NULL) return NULL;

    def->name = name ? strdup(name) : NULL;
    def->select_sql = sql ? strdup(sql) : NULL;
    def->type = type;
    def->column_names = NULL;
    def->column_count = 0;
    def->refresh = REFRESH_ON_DEMAND;
    def->refresh_interval = 0;
    def->with_check_option = false;

    return def;
}

void view_def_destroy(view_def_t *def)
{
    if (def == NULL) return;

    if (def->name) free(def->name);
    if (def->select_sql) free(def->select_sql);
    if (def->column_names) {
        for (int i = 0; i < def->column_count; i++) {
            if (def->column_names[i]) free(def->column_names[i]);
        }
        free(def->column_names);
    }
    free(def);
}

const char *view_type_name(view_type_t type)
{
    switch (type) {
        case VIEW_REGULAR:    return "regular";
        case VIEW_MATERIALIZED: return "materialized";
        default: return "unknown";
    }
}

const char *view_state_name(mv_state_t state)
{
    switch (state) {
        case MV_STATE_VALID:    return "valid";
        case MV_STATE_STALE:    return "stale";
        case MV_STATE_BUILDING: return "building";
        case MV_STATE_ERROR:    return "error";
        default: return "unknown";
    }
}