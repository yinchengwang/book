/*
 * trigger.h - 触发器接口
 */

#ifndef DB_TRIGGER_H
#define DB_TRIGGER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ─────────────────────────────────────────────────────────────────
 * 触发事件类型
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    TRIGGER_BEFORE_INSERT,   /* INSERT 之前 */
    TRIGGER_AFTER_INSERT,    /* INSERT 之后 */
    TRIGGER_BEFORE_UPDATE,   /* UPDATE 之前 */
    TRIGGER_AFTER_UPDATE,    /* UPDATE 之后 */
    TRIGGER_BEFORE_DELETE,   /* DELETE 之前 */
    TRIGGER_AFTER_DELETE,   /* DELETE 之后 */
    TRIGGER_BEFORE_SELECT,   /* SELECT 之前 */
    TRIGGER_AFTER_SELECT     /* SELECT 之后 */
} trigger_event_t;

/* ─────────────────────────────────────────────────────────────────
 * 触发器粒度
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    TRIGGER_ROW,    /* 行级触发器 */
    TRIGGER_STATEMENT  /* 语句级触发器 */
} trigger_level_t;

/* ─────────────────────────────────────────────────────────────────
 * 触发器时机
 * ───────────────────────────────────────────────────────────────── */

typedef enum {
    TRIGGER_FIRED_BEFORE,   /* 之前触发 */
    TRIGGER_FIRED_AFTER,    /* 之后触发 */
    TRIGGER_FIRED_INSTEAD_OF /* 替代触发（用于视图） */
} trigger_fired_t;

/* ─────────────────────────────────────────────────────────────────
 * 触发器定义
 * ───────────────────────────────────────────────────────────────── */

typedef struct trigger_def {
    char *name;              /* 触发器名称 */
    char *table_name;        /* 关联表名 */
    trigger_event_t event;   /* 触发事件 */
    trigger_level_t level;   /* 触发粒度 */
    char *when_clause;       /* WHEN 条件 */
    char *action;           /* 触发动作（SQL 或函数调用） */
    bool enabled;           /* 是否启用 */
} trigger_def_t;

/* ─────────────────────────────────────────────────────────────────
 * 触发器上下文（触发时可用）
 * ───────────────────────────────────────────────────────────────── */

typedef struct trigger_context {
    trigger_event_t event;      /* 触发事件 */
    trigger_fired_t fired;     /* 触发时机 */
    void *old_row;            /* 旧行（UPDATE/DELETE） */
    void *new_row;            /* 新行（INSERT/UPDATE） */
    void *trigger_data;       /* 触发器特定数据 */
} trigger_context_t;

/* ─────────────────────────────────────────────────────────────────
 * 触发器函数类型
 * ───────────────────────────────────────────────────────────────── */

typedef int (*trigger_func_t)(const trigger_context_t *ctx);

/* ─────────────────────────────────────────────────────────────────
 * 触发器句柄
 * ───────────────────────────────────────────────────────────────── */

typedef struct trigger trigger_t;

/* ─────────────────────────────────────────────────────────────────
 * 触发器管理器
 * ───────────────────────────────────────────────────────────────── */

typedef struct trigger_manager trigger_manager_t;

/**
 * @brief 创建触发器管理器
 * @return 触发器管理器
 */
trigger_manager_t *trigger_manager_create(void);

/**
 * @brief 销毁触发器管理器
 * @param mgr 触发器管理器
 */
void trigger_manager_destroy(trigger_manager_t *mgr);

/**
 * @brief 创建触发器
 * @param mgr 触发器管理器
 * @param def 触发器定义
 * @return 触发器句柄
 */
trigger_t *trigger_create(trigger_manager_t *mgr, const trigger_def_t *def);

/**
 * @brief 删除触发器
 * @param mgr 触发器管理器
 * @param name 触发器名
 * @return 0 成功
 */
int trigger_drop(trigger_manager_t *mgr, const char *name);

/**
 * @brief 获取触发器
 * @param mgr 触发器管理器
 * @param name 触发器名
 * @return 触发器句柄
 */
trigger_t *trigger_get(trigger_manager_t *mgr, const char *name);

/**
 * @brief 启用触发器
 * @param trigger 触发器
 * @return 0 成功
 */
int trigger_enable(trigger_t *trigger);

/**
 * @brief 禁用触发器
 * @param trigger 触发器
 * @return 0 成功
 */
int trigger_disable(trigger_t *trigger);

/**
 * @brief 检查触发器是否启用
 * @param trigger 触发器
 * @return true 启用
 */
bool trigger_is_enabled(const trigger_t *trigger);

/* ─────────────────────────────────────────────────────────────────
 * 触发器执行
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 触发前执行（调用触发器）
 * @param mgr 触发器管理器
 * @param table_name 表名
 * @param event 事件
 * @param ctx 上下文
 * @return 0 成功，-1 中断
 */
int trigger_fire_before(trigger_manager_t *mgr, const char *table_name,
                       trigger_event_t event, trigger_context_t *ctx);

/**
 * @brief 触发后执行
 * @param mgr 触发器管理器
 * @param table_name 表名
 * @param event 事件
 * @param ctx 上下文
 */
void trigger_fire_after(trigger_manager_t *mgr, const char *table_name,
                       trigger_event_t event, trigger_context_t *ctx);

/* ─────────────────────────────────────────────────────────────────
 * 触发器列表
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 获取表的触发器
 * @param mgr 触发器管理器
 * @param table_name 表名
 * @param count 输出：触发器数量
 * @return 触发器名数组（需手动释放）
 */
char **trigger_list_for_table(const trigger_manager_t *mgr,
                             const char *table_name, int *count);

/**
 * @brief 获取所有触发器
 * @param mgr 触发器管理器
 * @param count 输出：触发器数量
 * @return 触发器名数组（需手动释放）
 */
char **trigger_list_all(const trigger_manager_t *mgr, int *count);

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数
 * ───────────────────────────────────────────────────────────────── */

/**
 * @brief 创建触发器定义
 * @param name 名称
 * @param table 表名
 * @param event 事件
 * @param action 动作
 * @return 触发器定义
 */
trigger_def_t *trigger_def_create(const char *name, const char *table,
                                 trigger_event_t event, const char *action);

/**
 * @brief 销毁触发器定义
 * @param def 触发器定义
 */
void trigger_def_destroy(trigger_def_t *def);

/**
 * @brief 获取事件名称
 * @param event 事件
 * @return 名称
 */
const char *trigger_event_name(trigger_event_t event);

/**
 * @brief 获取触发器级别名称
 * @param level 级别
 * @return 名称
 */
const char *trigger_level_name(trigger_level_t level);

/**
 * @brief 获取触发时机名称
 * @param fired 触发时机
 * @return 名称
 */
const char *trigger_fired_name(trigger_fired_t fired);

#ifdef __cplusplus
}
#endif

#endif /* DB_TRIGGER_H */