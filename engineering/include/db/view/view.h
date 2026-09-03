/*
 * view.h - 视图接口
 */

#ifndef DB_VIEW_H
#define DB_VIEW_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 视图类型
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    VIEW_REGULAR,         /* 普通视图 */
    VIEW_MATERIALIZED    /* 物化视图 */
} view_type_t;

/* ─────────────────────────────────────────────────────────────────
 * 视图刷新策略
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    REFRESH_ON_DEMAND,   /* 按需刷新 */
    REFRESH_ON_COMMIT,   /* 提交时刷新 */
    REFRESH_INTERVAL     /* 定时刷新 */
} view_refresh_t;

/* ─────────────────────────────────────────────────────────────────
 * 视图定义
 * ───────────────────────────────────────────────────────────────── */

typedef struct view_def {
    char *name;               /* 视图名称 */
    view_type_t type;         /* 视图类型 */
    char *select_sql;        /* SELECT 查询 */
    char **column_names;     /* 列名数组 */
    int column_count;          /* 列数 */
    view_refresh_t refresh;  /* 刷新策略 */
    int refresh_interval;     /* 刷新间隔（秒） */
    bool with_check_option;   /* WITH CHECK OPTION */
} view_def_t;

/* ─────────────────────────────────────────────────────────────────
 * 物化视图状态
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    MV_STATE_VALID,       /* 有效 */
    MV_STATE_STALE,      /* 需要刷新 */
    MV_STATE_BUILDING,   /* 构建中 */
    MV_STATE_ERROR        /* 错误状态 */
} mv_state_t;

/* ─────────────────────────────────────────────────────────────────
 * 视图句柄
 * ───────────────────────────────────────────────────────────────── */

typedef struct view view_t;

/* ─────────────────────────────────────────────────────────────────
 * 视图管理器
 * ───────────────────────────────────────────────────────────────── */

typedef struct view_manager view_manager_t;

/**
 * @brief 创建视图管理器
 * @return 视图管理器
 */
view_manager_t *view_manager_create(void);

/**
 * @brief 销毁视图管理器
 * @param mgr 视图管理器
 */
void view_manager_destroy(view_manager_t *mgr);

/**
 * @brief 创建视图
 * @param mgr 视图管理器
 * @param def 视图定义
 * @return 视图句柄
 */
view_t *view_create(view_manager_t *mgr, const view_def_t *def);

/**
 * @brief 删除视图
 * @param mgr 视图管理器
 * @param name 视图名
 * @return 0 成功
 */
int view_drop(view_manager_t *mgr, const char *name);

/**
 * @brief 获取视图
 * @param mgr 视图管理器
 * @param name 视图名
 * @return 视图句柄
 */
view_t *view_get(view_manager_t *mgr, const char *name);

/**
 * @brief 检查视图是否存在
 * @param mgr 视图管理器
 * @param name 视图名
 * @return true 存在
 */
bool view_exists(const view_manager_t *mgr, const char *name);

/**
 * @brief 获取所有视图
 * @param mgr 视图管理器
 * @param count 输出：视图数量
 * @return 视图名数组（需手动释放）
 */
char **view_list(const view_manager_t *mgr, int *count);

/* ─────────────────────────────────────────────────────────────────
 * 物化视图操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 刷新物化视图
 * @param view 视图
 * @return 0 成功
 */
int view_refresh(view_t *view);

/**
 * @brief 异步刷新物化视图
 * @param view 视图
 * @return 0 成功
 */
int view_refresh_async(view_t *view);

/**
 * @brief 获取物化视图状态
 * @param view 视图
 * @return 状态
 */
mv_state_t view_get_state(const view_t *view);

/**
 * @brief 获取最后刷新时间
 * @param view 视图
 * @return 时间戳
 */
uint64_t view_last_refresh(const view_t *view);

/* ─────────────────────────────────────────────────────────────────
 * 视图定义操作
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取视图定义
 * @param view 视图
 * @return 视图定义
 */
const view_def_t *view_get_def(const view_t *view);

/**
 * @brief 获取视图类型
 * @param view 视图
 * @return 类型
 */
view_type_t view_get_type(const view_t *view);

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建视图定义
 * @param name 视图名
 * @param sql SELECT 语句
 * @param type 视图类型
 * @return 视图定义
 */
view_def_t *view_def_create(const char *name, const char *sql, view_type_t type);

/**
 * @brief 销毁视图定义
 * @param def 视图定义
 */
void view_def_destroy(view_def_t *def);

/**
 * @brief 获取视图类型名称
 * @param type 类型
 * @return 名称
 */
const char *view_type_name(view_type_t type);

/**
 * @brief 获取物化视图状态名称
 * @param state 状态
 * @return 名称
 */
const char *view_state_name(mv_state_t state);

#ifdef __cplusplus
}
#endif

#endif /* DB_VIEW_H */