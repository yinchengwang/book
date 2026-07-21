#ifndef TODO_MODEL_H
#define TODO_MODEL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * 字段长度常量
 * ============================================================ */
#define TODO_TITLE_MAX    256
#define TODO_DESC_MAX     4096
#define TODO_STATUS_MAX   16
#define TODO_LABELS_MAX   1024
#define CHECKLIST_TEXT_MAX 512
#define GROUP_NAME_MAX    64
#define GROUP_COLOR_MAX   16
#define COMMENT_TEXT_MAX  2048

/* ============================================================
 * 优先级定义（0-4）
 * ============================================================ */
#define PRIORITY_URGENT   0  /* 紧急 */
#define PRIORITY_HIGH     1  /* 高 */
#define PRIORITY_MEDIUM   2  /* 中 */
#define PRIORITY_LOW      3  /* 低 */
#define PRIORITY_NONE     4  /* 无优先级 */

/* ============================================================
 * 待办事项结构体
 * ============================================================ */
typedef struct {
    int64_t id;
    char    title[TODO_TITLE_MAX];
    char    description[TODO_DESC_MAX];
    char    status[TODO_STATUS_MAX];    /* "open" / "closed" / "archived" */
    char    labels[TODO_LABELS_MAX];    /* JSON 数组字符串 */
    int     priority;                   /* 0-4: 0=紧急 1=高 2=中 3=低 4=无 */
    int64_t due_date;                   /* Unix 时间戳，0=无截止日期 */
    int64_t group_id;                   /* 分组 ID，0=未分组 */
    int     sort_order;                 /* 排序序号 */
    int64_t created_at;
    int64_t updated_at;
    /* ----- 新增字段 ----- */
    int     todo_type;                /* 0=普通 1=每日 2=每周 3=每月 */
    int64_t original_date;            /* 初始排定日期 */
    int     carryover_count;          /* 累计顺延次数 */
    int64_t plan_id;                  /* 所属学习计划 */
    int64_t plan_item_id;             /* 所属计划项 */
    int64_t completed_at;             /* 完成时间，0=未完成 */
    int64_t postpone_until;           /* 主动延后到的目标日期 */
    int64_t task_system_id;           /* 所属任务系统，0=默认 */
} todo_t;

/* 全局状态（外部文件可访问，用于日历等功能） */
extern todo_t *g_todos;
extern int g_todo_count;

/* ============================================================
 * 任务系统结构体（顶层容器，每个系统管理自己的 todo 列表）
 * ============================================================ */
#define TASK_SYSTEM_NAME_MAX 64

typedef struct {
    int64_t id;
    char    name[TASK_SYSTEM_NAME_MAX];
    char    description[1024];
    char    color[16];
    int     sort_order;
    int64_t created_at;
} task_system_t;

/* ============================================================
 * 学习计划结构体
 * ============================================================ */
#define PLAN_NAME_MAX    128
#define PLAN_DESC_MAX    4096
#define ITEM_TITLE_MAX   256

typedef struct {
    int64_t id;
    char    name[PLAN_NAME_MAX];
    char    description[PLAN_DESC_MAX];
    int64_t start_date;
    int64_t end_date;
    char    color[16];
    int     status;          /* 0=进行中 1=已完成 2=已暂停 */
    int64_t created_at;
    int64_t updated_at;
} plan_t;

typedef struct {
    int64_t id;
    int64_t plan_id;
    int64_t parent_id;       /* 0=根 */
    char    title[ITEM_TITLE_MAX];
    int     item_type;       /* 1=阶段 2=模块 3=具体任务 */
    int64_t planned_date;
    int     estimated_minutes;
    int     order_index;
    int     completion_rule; /* 1=按日 2=按周 3=按月 */
    int64_t todo_id;         /* 展开后关联的 todo */
    int     actual_minutes;  /* 实际耗时 */
} plan_item_t;

/* ============================================================
 * 待办清单项结构体
 * ============================================================ */
typedef struct {
    int64_t id;
    int64_t todo_id;                   /* 从 issue_id 重命名而来 */
    char    text[CHECKLIST_TEXT_MAX];
    int     done;                      /* 0=未完成 1=已完成 */
    int     sort_order;
} checklist_item_t;

/* ============================================================
 * 分组结构体
 * ============================================================ */
typedef struct {
    int64_t id;
    char    name[GROUP_NAME_MAX];
    char    color[GROUP_COLOR_MAX];    /* 十六进制颜色值，如 "#f85149" */
    int     sort_order;
    int64_t created_at;
} group_t;

/* ============================================================
 * 评论结构体
 * ============================================================ */
typedef struct {
    int64_t id;
    int64_t todo_id;
    char    text[COMMENT_TEXT_MAX];
    int64_t created_at;
} comment_t;

/* ============================================================
 * 筛选/排序规则
 * ============================================================ */
#define FILTER_VALUE_MAX 256
#define FILTER_OP_MAX 16

typedef struct {
    int64_t field_id;                    /* 字段 ID */
    char    operator[FILTER_OP_MAX];      /* eq, ne, gt, lt, gte, lte, contains, is_empty, is_not_empty */
    char    value[FILTER_VALUE_MAX];      /* 筛选值（字符串形式） */
} filter_rule_t;

#define MAX_FILTERS 16
#define MAX_SORTS 8

typedef struct {
    int64_t field_id;
    char    direction[4];                /* asc / desc */
} sort_rule_t;

/* ============================================================
 * 查询参数结构体
 * ============================================================ */
typedef struct {
    const char *status;
    const char *labels;
    const char *search;
    int         priority;               /* -1=全部 */
    int64_t     group_id;              /* -1=全部，0=未分组 */
    int64_t     due_before;            /* 截止日期早于此时间戳 */
    int64_t     due_after;             /* 截止日期晚于此时间戳 */
    const char *sort;                  /* 排序字段：created_at, due_date, priority, sort_order */
    int         sort_desc;             /* 0=升序 1=降序 */
    int         page;
    int         per_page;

    /* 新增 */
    int64_t     view_id;               /* 视图 ID，非 0 时从视图配置加载 filter/sort */
    int         filter_count;          /* filter_rules 中的有效数量 */
    filter_rule_t filter_rules[MAX_FILTERS];
    int         sort_count;            /* sort_rules 中的有效数量 */
    sort_rule_t sort_rules[MAX_SORTS];
} todo_query_t;

/* ============================================================
 * 查询结果结构体
 * ============================================================ */
typedef struct {
    todo_t *items;
    int     count;
    int     total;
} todo_list_t;

/* ============================================================
 * 统计数据结构体
 * ============================================================ */
typedef struct {
    int total;
    int open;
    int closed;
    int archived;
    int overdue;
    int due_today;
    double completion_rate;            /* 完成率 0.0-1.0 */
} todo_stats_t;

/* ============================================================
 * 按优先级/分组的统计项
 * ============================================================ */
typedef struct {
    int priority;
    int count;
} priority_stat_t;

typedef struct {
    int64_t group_id;
    char    group_name[GROUP_NAME_MAX];
    int     count;
} group_stat_t;

/* ============================================================
 * 数据库生命周期
 * ============================================================ */

/**
 * @brief 加载/初始化数据库（从 JSON 文件）
 * @param db_path 数据文件路径（.json）
 * @return 0 成功，-1 失败
 */
int todo_db_load(const char *db_path);

/**
 * @brief 显式保存（通常自动保存）
 */
int todo_db_save(void);

/**
 * @brief 释放资源
 */
void todo_db_shutdown(void);

/**
 * @brief 重置数据库状态（用于测试）
 */
void todo_db_reset(void);

/**
 * @brief 根据 ID 查找待办事项在数组中的索引
 * @param id 待办 ID
 * @return 索引位置（>=0），未找到返回 -1
 */
int find_todo_idx(int64_t id);

/* ============================================================
 * Todo CRUD
 * ============================================================ */

/**
 * @brief 创建待办事项
 * @param todo 待办数据（id 会被忽略，由系统分配）
 * @param out_id 输出新创建的 ID
 * @return 0 成功，-1 失败
 */
int todo_create(const todo_t *todo, int64_t *out_id);

/**
 * @brief 根据 ID 获取待办事项
 * @param id 待办 ID
 * @param todo 输出数据
 * @return 0 成功，-1 未找到
 */
int todo_get_by_id(int64_t id, todo_t *todo);

/**
 * @brief 更新待办事项
 * @param todo 待办数据（必须包含有效 id）
 * @return 0 成功，-1 失败
 */
int todo_update(const todo_t *todo);

/**
 * @brief 删除待办事项（级联删除 checklist）
 * @param id 待办 ID
 * @return 0 成功，-1 未找到
 */
int todo_delete(int64_t id);

/**
 * @brief 查询待办事项列表
 * @param query 查询参数
 * @param result 输出结果
 * @return 0 成功
 */
int todo_list(const todo_query_t *query, todo_list_t *result);

/**
 * @brief 释放查询结果
 */
void todo_list_free(todo_list_t *result);

/**
 * @brief 更新排序序号
 * @param id 待办 ID
 * @param sort_order 新的排序序号
 * @return 0 成功
 */
int todo_update_sort(int64_t id, int sort_order);

/**
 * @brief 获取统计数据
 * @param stats 输出统计数据
 * @return 0 成功
 */
int todo_get_stats(todo_stats_t *stats);

/* ============================================================
 * Checklist
 * ============================================================ */

/**
 * @brief 添加清单项
 * @param todo_id 所属待办 ID
 * @param text 清单文本
 * @param item 输出创建的清单项
 * @return 0 成功
 */
int checklist_add(int64_t todo_id, const char *text, checklist_item_t *item);

/**
 * @brief 切换清单项完成状态
 * @param item_id 清单项 ID
 * @return 0 成功
 */
int checklist_toggle(int64_t item_id);

/**
 * @brief 删除清单项
 * @param item_id 清单项 ID
 * @return 0 成功
 */
int checklist_delete(int64_t item_id);

/**
 * @brief 列出某待办的所有清单项
 * @param todo_id 待办 ID
 * @param items 输出清单项数组
 * @param count 输出清单项数量
 * @return 0 成功
 */
int checklist_list(int64_t todo_id, checklist_item_t **items, int *count);

/**
 * @brief 释放清单项数组
 */
void checklist_list_free(checklist_item_t *items, int count);

/* ============================================================
 * 分组 CRUD
 * ============================================================ */

/**
 * @brief 创建分组
 * @param group 分组数据
 * @param out_id 输出新创建的 ID
 * @return 0 成功
 */
int group_create(const group_t *group, int64_t *out_id);

/**
 * @brief 获取分组
 * @param id 分组 ID
 * @param group 输出分组数据
 * @return 0 成功
 */
int group_get(int64_t id, group_t *group);

/**
 * @brief 更新分组
 * @param group 分组数据（必须包含有效 id）
 * @return 0 成功
 */
int group_update(const group_t *group);

/**
 * @brief 删除分组（待办不删除，只是 group_id 置为 0）
 * @param id 分组 ID
 * @return 0 成功
 */
int group_delete(int64_t id);

/**
 * @brief 列出所有分组
 * @param groups 输出分组数组（需调用 group_list_free 释放）
 * @param count 输出分组数量
 * @return 0 成功
 */
int group_list(group_t **groups, int *count);

/**
 * @brief 释放分组数组
 */
void group_list_free(group_t *groups, int count);

/* ============================================================
 * 评论 CRUD
 * ============================================================ */

/**
 * @brief 添加评论
 * @param todo_id 待办 ID
 * @param text 评论文本
 * @param comment 输出创建的评论
 * @return 0 成功
 */
int comment_add(int64_t todo_id, const char *text, comment_t *comment);

/**
 * @brief 列出某待办的所有评论
 * @param todo_id 待办 ID
 * @param comments 输出评论数组（需调用 comment_list_free 释放）
 * @param count 输出评论数量
 * @return 0 成功
 */
int comment_list(int64_t todo_id, comment_t **comments, int *count);

/**
 * @brief 释放评论数组
 */
void comment_list_free(comment_t *comments, int count);

/**
 * @brief 删除评论
 * @param comment_id 评论 ID
 * @return 0 成功
 */
int comment_delete(int64_t comment_id);

/* ============================================================
 * 任务系统 CRUD
 * ============================================================ */
int task_system_create(const task_system_t *ts, int64_t *out_id);
int task_system_get(int64_t id, task_system_t *ts);
int task_system_update(const task_system_t *ts);
int task_system_delete(int64_t id);
int task_system_list(task_system_t **systems, int *count);
void task_system_list_free(task_system_t *systems, int count);

/* ============================================================
 * 学习计划 CRUD
 * ============================================================ */
int plan_create(const plan_t *plan, int64_t *out_id);
int plan_get(int64_t id, plan_t *plan);
int plan_update(const plan_t *plan);
int plan_delete(int64_t id);
int plan_list(plan_t **plans, int *count);
void plan_list_free(plan_t *plans, int count);

/* ============================================================
 * 计划项 CRUD（树形结构，自动按 parent_id 组织）
 * ============================================================ */
int plan_item_create(const plan_item_t *item, int64_t *out_id);
int plan_item_get(int64_t id, plan_item_t *item);
int plan_item_update(const plan_item_t *item);
int plan_item_delete(int64_t id);
int plan_item_list_by_plan(int64_t plan_id, plan_item_t **items, int *count);
void plan_item_list_free(plan_item_t *items, int count);

/* ============================================================
 * 批量操作
 * ============================================================ */
int todo_batch_update_status(const int64_t *ids, int count, const char *status);
int todo_batch_delete(const int64_t *ids, int count);

#ifdef __cplusplus
}
#endif

#endif /* TODO_MODEL_H */
