# Todo App 日历与学习计划扩展 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** 扩展现有 todo-app（C 后端 + Vue 3 前端），增加日历三视图、智能顺延、任务系统、多粒度 TODO（日/周/月）、DFX 统计看板，并预置"关系型数据库完全学习计划"种子数据。

**Architecture:** 后端新增 3 个 C 模块（todo_calendar.c/h、todo_stats.c/h、todo_plan.c/h），在现有 todo_model.c/h 扩展数据模型，在 todo_handler.c 新增路由。前端新增 4 个视图（CalendarView、StatsDFXView、PlanManageView、TaskSystemManage），后端无外部依赖变更。

**Tech Stack:** C11 (MSVC/Winsock) + cJSON + Vue 3 + Vite

## Global Constraints

- 保留所有现有 todo-app 功能不变，只做扩展
- 老数据自动迁移（启动时检测 old JSON → 新格式）
- 所有 API 保持统一的 JSON 响应格式 `{"code":0,"data":{},"msg":"ok"}`
- 前端路由 `/` `/board` `/stats` `/groups` 不变，新增路由 `/calendar` `/stats-dfx` `/plan-manage` `/task-systems`
- 任务排序核心键为 `original_date`（初始排定日），顺延后不变
- 顺延只自动在"周内"边界内，跨周/跨月需用户手动确认
- 后台定时任务在独立线程运行，不阻塞 HTTP 主循环
- 预置学习计划 seed data 从 2026-07-21 开始排至 2027-05-20

---

## 文件结构

### 后端新增/修改

| 文件 | 操作 | 职责 |
|------|------|------|
| `todo_model.h` | 扩展 | 新增字段 + 新数据结构声明 |
| `todo_model.c` | 扩展 | 新数据持久化 + CRUD |
| `todo_handler.c` | 扩展 | 新增路由处理 |
| `todo_calendar.c` | 新建 | 日历查询 + 顺延逻辑 + 后台定时任务 |
| `todo_calendar.h` | 新建 | 日历模块接口 |
| `todo_stats.c` | 新建 | DFX 统计计算 |
| `todo_stats.h` | 新建 | DFX 统计接口 |
| `todo_plan.c` | 新建 | 计划模板 + 展开为 Todo |
| `todo_plan.h` | 新建 | 计划模块接口 |
| `todo_migration.h` | 扩展 | 迁移到新格式 |
| `todo_migration.c` | 扩展 | 新格式迁移逻辑 |
| `main.c` | 修改 | 启动后台定时任务线程 |

### 前端新增/修改

| 文件 | 操作 | 职责 |
|------|------|------|
| `src/api.js` | 扩展 | 新增日历/统计/计划 API 方法 |
| `src/router/index.js` | 扩展 | 新增路由 |
| `src/App.vue` | 修改 | 导航栏 + 过期任务弹窗检测 |
| `src/views/CalendarView.vue` | 新建 | 日历主视图 |
| `src/views/StatsDFXView.vue` | 新建 | DFX 统计页面 |
| `src/views/PlanManageView.vue` | 新建 | 任务系统 + 计划管理 |
| `src/components/CarryoverModal.vue` | 新建 | 过期任务批量确认弹窗 |
| `src/components/HeatmapChart.vue` | 新建 | 热力图组件 |

---

## P1：后端数据模型扩展 + 任务系统 CRUD

### Task 1.1: 扩展 todo_model 数据模型

**Files:**
- Modify: `engineering/apps/todo-app/todo_model.h`
- Modify: `engineering/apps/todo-app/todo_model.c`

**Interfaces:**
- Produces: 新增 `task_system_t`、`plan_t`、`plan_item_t` 结构体；todo_t 新增 7 个字段

- [ ] **Step 1: 在 todo_model.h 新增 task_system_t 结构体**

在 `todo_t` 之后、`todo_query_t` 之前插入：

```c
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
```

- [ ] **Step 2: 在 todo_model.h 新增 plan_t 和 plan_item_t 结构体**

```c
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
```

- [ ] **Step 3: 在 todo_t 中新增 7 个字段**

```c
    /* ----- 新增字段 ----- */
    int     todo_type;                /* 0=普通 1=每日 2=每周 3=每月 */
    int64_t original_date;            /* 初始排定日期 */
    int     carryover_count;          /* 累计顺延次数 */
    int64_t plan_id;                  /* 所属学习计划 */
    int64_t plan_item_id;             /* 所属计划项 */
    int64_t completed_at;             /* 完成时间，0=未完成 */
    int64_t postpone_until;           /* 主动延后到的目标日期 */
    int64_t task_system_id;           /* 所属任务系统，0=默认 */
```

- [ ] **Step 4: 在 todo_model.h 添加新 CRUD 声明**

```c
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
```

- [ ] **Step 5: 在 todo_model.c 实现 task_system CRUD**

在 `g_comments` 之后添加全局状态：

```c
static task_system_t *g_task_systems = NULL;
static int            g_ts_count = 0;
static int            g_ts_cap = 0;
static int64_t        g_next_ts_id = 1;

static plan_t        *g_plans = NULL;
static int            g_plan_count = 0;
static int            g_plan_cap = 0;
static int64_t        g_next_plan_id = 1;

static plan_item_t   *g_plan_items = NULL;
static int            g_pi_count = 0;
static int            g_pi_cap = 0;
static int64_t        g_next_pi_id = 1;
```

实现 `task_system_create/get/update/delete/list`，模式与 `group_xxx` 完全一致。

- [ ] **Step 6: 在 todo_model.c 实现 plan CRUD + plan_item CRUD**

模式同行 `group_xxx`，注意 plan_item 按 plan_id + parent_id 树形组织。

- [ ] **Step 7: 修改 todo_db_load/persist 处理新表**

在 `persist_now()` 中添加：

```c
cJSON_AddItemToObject(root, "task_systems", task_systems_to_json());
cJSON_AddItemToObject(root, "plans", plans_to_json());
cJSON_AddItemToObject(root, "plan_items", plan_items_to_json());
```

在 `todo_db_load()` 中添加对应的 JSON 解析。新增默认 task_system（id=1, "默认任务系统"）。

- [ ] **Step 8: 编译验证**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -20
```

预期：编译成功，无警告。

- [ ] **Step 9: 提交**

```bash
git add engineering/apps/todo-app/
git commit -m "feat: 扩展数据模型，新增任务系统/学习计划/计划项结构体"
git push
```

---

### Task 1.2: 后端子模块文件创建（todo_calendar/todo_stats/todo_plan）

**Files:**
- Create: `engineering/apps/todo-app/todo_calendar.h`
- Create: `engineering/apps/todo-app/todo_calendar.c`
- Create: `engineering/apps/todo-app/todo_stats.h`
- Create: `engineering/apps/todo-app/todo_stats.c`
- Create: `engineering/apps/todo-app/todo_plan.h`
- Create: `engineering/apps/todo-app/todo_plan.c`
- Modify: `engineering/apps/todo-app/CMakeLists.txt`

- [ ] **Step 1: 创建 todo_calendar.h**

```c
#ifndef TODO_CALENDAR_H
#define TODO_CALENDAR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 日历查询参数 */
typedef struct {
    int64_t date;       /* 参考日期（Unix 时间戳） */
    int64_t task_system_id; /* -1=全部, 0=默认系统 */
} calendar_query_t;

/* 顺延确认项 */
typedef struct {
    int64_t todo_id;
    int     completed;  /* 0=未完成→顺延 1=已完成 */
} carryover_confirm_t;

/* 顺延报告 */
typedef struct {
    int carried_in_week;
    int overflow_to_next_week;
    int week_promotable;
    int carried_in_month;
    int overflow_to_next_month;
    int month_promotable;
} carryover_report_t;

/* 初始化/关闭 */
void calendar_init(void);
void calendar_shutdown(void);

/* 启动后台定时任务线程 */
void calendar_timer_start(void);

/* 停止后台定时任务线程 */
void calendar_timer_stop(void);

/* 查询某天的所有 todo */
int calendar_day_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count);

/* 查询某周的所有 todo（按日期分组） */
int calendar_week_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count);

/* 查询某月的所有 todo */
int calendar_month_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count);

/* 获取待确认的过期任务 */
int calendar_pending_carryover(todo_t **todos, int *count);

/* 批量确认过期任务 */
carryover_report_t calendar_confirm_carryover(carryover_confirm_t *items, int count);

/* 延后某个任务 */
int calendar_postpone(int64_t todo_id, int days);

/* 提升下周/下月任务到本周/本月 */
carryover_report_t calendar_promote(int scope);

#ifdef __cplusplus
}
#endif

#endif /* TODO_CALENDAR_H */
```

- [ ] **Step 2: 创建 todo_stats.h**

```c
#ifndef TODO_STATS_H
#define TODO_STATS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 周趋势项 */
typedef struct {
    int     week_offset;  /* 0=本周, -1=上周... */
    int64_t week_start;
    int64_t week_end;
    int     completed_count;
} week_trend_t;

/* DFX 统计结果 */
typedef struct {
    int total_completed;
    int on_time_completed;
    int early_completed;
    int carried_over_count;
    int overdue_count;
    double completion_rate;
    int streak_days;
    double avg_early_days;
    double avg_carryover_days;
    int     weekly_trend_count;
    week_trend_t weekly_trend[12]; /* 近 12 周 */
    double plan_completion_rate;
    double temporary_completion_rate;
    int     phase_progress_count;
    double phase_progress[8];    /* 各阶段完成百分比 */
    double plan_health_score;
    double estimation_accuracy;
    double hard_task_ratio;
} dfx_stats_t;

/* 热力图格子 */
typedef struct {
    int64_t date;
    int     completed_count;
    int     level;  /* 0-3 */
} heatmap_cell_t;

/* 热力图周 */
typedef struct {
    int64_t         week_start;
    heatmap_cell_t  days[7];
} heatmap_week_t;

/* 热力图数据 */
typedef struct {
    int             weeks_count;
    heatmap_week_t  weeks[53]; /* 一年最多 53 周 */
    int             max_daily_count;
} heatmap_data_t;

/* 计算 DFX 统计 */
int stats_calculate(dfx_stats_t *stats);

/* 计算热力图 */
int stats_calculate_heatmap(heatmap_data_t *heatmap);

#ifdef __cplusplus
}
#endif

#endif /* TODO_STATS_H */
```

- [ ] **Step 3: 创建 todo_plan.h**

```c
#ifndef TODO_PLAN_H
#define TODO_PLAN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 预置模板标识 */
#define TEMPLATE_DB_LEARNING 1

/* 导入预置模板 */
int plan_import_template(int template_id, int64_t start_date, int64_t task_system_id);

/* 展开计划为 Todo */
int plan_expand_to_todos(int64_t plan_id, int64_t task_system_id);

/* 获取计划中的阶段进度 */
int plan_get_phase_progress(int64_t plan_id, double *progress, int *count);

#ifdef __cplusplus
}
#endif

#endif /* TODO_PLAN_H */
```

- [ ] **Step 4: 创建空实现文件**

```c
/* todo_calendar.c — 暂存桩 */
#include "todo_calendar.h"
#include "todo_model.h"
#include <stdlib.h>
#include <string.h>

void calendar_init(void) {}
void calendar_shutdown(void) {}
void calendar_timer_start(void) {}
void calendar_timer_stop(void) {}

int calendar_day_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    *todos = NULL; *count = 0; return 0;
}
int calendar_week_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    *todos = NULL; *count = 0; return 0;
}
int calendar_month_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    *todos = NULL; *count = 0; return 0;
}
int calendar_pending_carryover(todo_t **todos, int *count) { *todos = NULL; *count = 0; return 0; }
carryover_report_t calendar_confirm_carryover(carryover_confirm_t *items, int count) {
    carryover_report_t r = {0}; return r;
}
int calendar_postpone(int64_t todo_id, int days) { return 0; }
carryover_report_t calendar_promote(int scope) { carryover_report_t r = {0}; return r; }
```

```c
/* todo_stats.c — 暂存桩 */
#include "todo_stats.h"
int stats_calculate(dfx_stats_t *stats) { memset(stats, 0, sizeof(*stats)); return 0; }
int stats_calculate_heatmap(heatmap_data_t *heatmap) { memset(heatmap, 0, sizeof(*heatmap)); return 0; }
```

```c
/* todo_plan.c — 暂存桩 */
#include "todo_plan.h"
int plan_import_template(int template_id, int64_t start_date, int64_t task_system_id) { return 0; }
int plan_expand_to_todos(int64_t plan_id, int64_t task_system_id) { return 0; }
int plan_get_phase_progress(int64_t plan_id, double *progress, int *count) { *count = 0; return 0; }
```

- [ ] **Step 5: 编译验证**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -10
```

预期：编译成功。

- [ ] **Step 6: 提交**

```bash
git add engineering/apps/todo-app/
git commit -m "feat: 创建日历/统计/计划模块骨架文件"
git push
```

---

### Task 1.3: 任务系统 API 路由

**Files:**
- Modify: `engineering/apps/todo-app/todo_handler.c`

**Interfaces:**
- Produces: `GET/POST /api/task-systems`, `PATCH/DELETE /api/task-systems/:id`

- [ ] **Step 1: 添加 task_system_to_json 序列化函数**

与 `group_to_json` 模式一致：

```c
static cJSON *task_system_to_json(const task_system_t *ts) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "id", ts->id);
    cJSON_AddStringToObject(j, "name", ts->name);
    cJSON_AddStringToObject(j, "description", ts->description);
    cJSON_AddStringToObject(j, "color", ts->color);
    cJSON_AddNumberToObject(j, "sort_order", ts->sort_order);
    cJSON_AddNumberToObject(j, "created_at", ts->created_at);
    return j;
}
```

- [ ] **Step 2: 添加 handle 函数**

按 `handle_group_list/create/update/delete` 模式，实现 `handle_task_system_list/create/update/delete`。

- [ ] **Step 3: 在 handle_request 路由表中添加**

在分组路由之前添加：

```c
    /* GET /api/task-systems */
    else if (strcmp(path, "/api/task-systems") == 0) {
        ...
    }
    /* /api/task-systems/:id */
    else if (sscanf(path, "/api/task-systems/%lld", ...) ...
```

- [ ] **Step 4: 编译 + 提交**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
git add engineering/apps/todo-app/
git commit -m "feat: 任务系统 CRUD API 路由"
git push
```

---

## P2：后端日历 API + 顺延逻辑 + 后台定时任务 + DFX API

### Task 2.1: 实现日历查询（日/周/月）

**Files:**
- Modify: `engineering/apps/todo-app/todo_calendar.c`
- Modify: `engineering/apps/todo-app/todo_calendar.h`

**Interfaces:**
- Consumes: `todo_t`（含新字段 `original_date`, `carryover_count`, `task_system_id`）
- Produces: `calendar_day_todos`, `calendar_week_todos`, `calendar_month_todos`

- [ ] **Step 1: 实现日期工具函数**

```c
/* 获取今天的 Unix 时间戳（00:00:00） */
static int64_t today_start(void) {
    return (int64_t)(time(NULL) / 86400) * 86400;
}

/* 获取某天的 00:00:00 */
static int64_t day_start(int64_t ts) {
    return (ts / 86400) * 86400;
}

/* 获取某天是周几（0=周日, 1=周一, ...） */
static int day_of_week(int64_t ts) {
    struct tm *t = localtime((time_t *)&ts);
    int wday = t->tm_wday;  /* 0=周日 */
    return wday == 0 ? 7 : wday; /* 转为 1=周一, 7=周日 */
}

/* 获取某周的周一 00:00:00 */
static int64_t week_start(int64_t ts) {
    int64_t ds = day_start(ts);
    int dow = day_of_week(ds);
    return ds - (int64_t)(dow - 1) * 86400;
}

/* 获取某月的 1 号 00:00:00 */
static int64_t month_start(int64_t ts) {
    struct tm *t = localtime((time_t *)&ts);
    struct tm copy = *t;
    copy.tm_mday = 1;
    copy.tm_hour = 0; copy.tm_min = 0; copy.tm_sec = 0;
    return (int64_t)mktime(&copy);
}

/* 获取某月的天数 */
static int month_days(int64_t ts) {
    struct tm *t = localtime((time_t *)&ts);
    struct tm copy = *t;
    copy.tm_mon++; if (copy.tm_mon > 11) { copy.tm_mon = 0; copy.tm_year++; }
    copy.tm_mday = 1;
    copy.tm_hour = 0; copy.tm_min = 0; copy.tm_sec = 0;
    time_t next = mktime(&copy);
    return (int)((next - ts) / 86400);
}
```

- [ ] **Step 2: 实现日历查询函数**

```c
int calendar_day_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ds = day_start(date);
    int64_t de = ds + 86400;
    // 遍历 g_todos，筛选 due_date 在 [ds, de) 内的任务
    // 按 original_date + priority + sort_order 排序
    // 按 ts_id 筛选（-1=全部, 0=默认系统, >0=指定系统）
}

int calendar_week_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ws = week_start(date);
    int64_t we = ws + 7 * 86400;
    // 同上，筛选 due_date 在 [ws, we) 内的任务
}

int calendar_month_todos(int64_t date, int64_t ts_id, todo_t **todos, int *count) {
    int64_t ms = month_start(date);
    int64_t me = ms + (int64_t)month_days(date) * 86400;
    // 同上，筛选 due_date 在 [ms, me) 内的任务
}
```

- [ ] **Step 3: 编译验证**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
```

- [ ] **Step 4: 提交**

```bash
git add engineering/apps/todo-app/todo_calendar.c engineering/apps/todo-app/todo_calendar.h
git commit -m "feat: 实现日历日/周/月查询函数"
git push
```

### Task 2.2: 实现顺延逻辑 + 后台定时任务

**Files:**
- Modify: `engineering/apps/todo-app/todo_calendar.c`
- Modify: `engineering/apps/todo-app/todo_calendar.h`
- Modify: `engineering/apps/todo-app/main.c`

- [ ] **Step 1: 实现 pending_carryover 扫描**

```c
/* 全局标记：有待确认的过期任务 */
static int        g_pending_cc_count = 0;
static int64_t   *g_pending_cc_ids = NULL;  /* 过期任务 ID 数组 */
static int        g_pending_cc_cap = 0;

/* 扫描所有过期任务 */
static void scan_overdue_todos(void) {
    g_pending_cc_count = 0;
    int64_t now = today_start();
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") == 0 && 
            g_todos[i].due_date > 0 && g_todos[i].due_date < now) {
            // 确保不重复加入
            int already = 0;
            for (int j = 0; j < g_pending_cc_count; j++) {
                if (g_pending_cc_ids[j] == g_todos[i].id) { already = 1; break; }
            }
            if (!already) {
                if (g_pending_cc_count >= g_pending_cc_cap) {
                    g_pending_cc_cap = g_pending_cc_cap ? g_pending_cc_cap * 2 : 64;
                    g_pending_cc_ids = realloc(g_pending_cc_ids, g_pending_cc_cap * sizeof(int64_t));
                }
                g_pending_cc_ids[g_pending_cc_count++] = g_todos[i].id;
            }
        }
    }
}

int calendar_pending_carryover(todo_t **todos, int *count) {
    if (g_pending_cc_count == 0) { *todos = NULL; *count = 0; return 0; }
    *todos = malloc(g_pending_cc_count * sizeof(todo_t));
    *count = 0;
    for (int i = 0; i < g_pending_cc_count; i++) {
        int idx = find_todo_idx(g_pending_cc_ids[i]);
        if (idx >= 0) {
            (*todos)[*count] = g_todos[idx];
            (*count)++;
        }
    }
    return 0;
}
```

- [ ] **Step 2: 实现批量确认函数**

```c
carryover_report_t calendar_confirm_carryover(carryover_confirm_t *items, int count) {
    carryover_report_t report = {0};
    int64_t now = today_start();
    for (int i = 0; i < count; i++) {
        int idx = find_todo_idx(items[i].todo_id);
        if (idx < 0) continue;
        if (items[i].completed) {
            // 标记完成
            strcpy(g_todos[idx].status, "closed");
            g_todos[idx].completed_at = time(NULL);
        } else {
            // 顺延到今天
            g_todos[idx].due_date = now;
            g_todos[idx].carryover_count++;
            report.carried_in_week++;
        }
        g_todos[idx].updated_at = time(NULL);
    }
    // 清空待确认列表
    g_pending_cc_count = 0;
    todo_db_save();
    return report;
}
```

- [ ] **Step 3: 实现后台定时任务线程**

```c
#include <windows.h>
static HANDLE g_timer_thread = NULL;
static volatile int g_timer_running = 0;

static int64_t ms_until_tomorrow(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    int64_t ms_passed = (int64_t)t->tm_hour * 3600000 + (int64_t)t->tm_min * 60000 + (int64_t)t->tm_sec * 1000;
    int64_t ms_remaining = 86400000 - ms_passed;
    return ms_remaining > 0 ? ms_remaining : 1000;
}

static DWORD WINAPI timer_thread_func(LPVOID arg) {
    (void)arg;
    while (g_timer_running) {
        int64_t wait_ms = ms_until_tomorrow();
        Sleep((DWORD)wait_ms);
        if (!g_timer_running) break;
        scan_overdue_todos();
    }
    return 0;
}

void calendar_timer_start(void) {
    if (g_timer_running) return;
    g_timer_running = 1;
    DWORD tid;
    g_timer_thread = CreateThread(NULL, 0, timer_thread_func, NULL, 0, &tid);
    // 启动时立即扫描一次
    scan_overdue_todos();
}

void calendar_timer_stop(void) {
    g_timer_running = 0;
    if (g_timer_thread) {
        WaitForSingleObject(g_timer_thread, 3000);
        CloseHandle(g_timer_thread);
        g_timer_thread = NULL;
    }
}
```

- [ ] **Step 4: 在 main.c 中启动后台定时任务**

在 `todo_db_load()` 之后、`todo_handler_init()` 之前添加：

```c
#include "todo_calendar.h"

// 在 main() 中 signal_handler 注册之后：
calendar_timer_start();

// 在 http_server_start 返回后：
calendar_timer_stop();
```

- [ ] **Step 5: 实现 postpone 和 promote**

```c
int calendar_postpone(int64_t todo_id, int days) {
    int idx = find_todo_idx(todo_id);
    if (idx < 0) return -1;
    g_todos[idx].due_date += (int64_t)days * 86400;
    g_todos[idx].postpone_until = g_todos[idx].due_date;
    g_todos[idx].updated_at = time(NULL);
    todo_db_save();
    return 0;
}

carryover_report_t calendar_promote(int scope) {
    // scope=1: 提升下周任务到本周
    // scope=2: 提升下月任务到本月
    // 找到 scope 对应的下一个周期中的 open 任务
    // 将其 due_date 调整到当前周期末尾
    // 返回报告
    carryover_report_t r = {0};
    return r;
}
```

- [ ] **Step 6: 编译 + 提交**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
git add engineering/apps/todo-app/
git commit -m "feat: 实现顺延逻辑 + 后台定时任务线程"
git push
```

### Task 2.3: 实现日历 API 路由

**Files:**
- Modify: `engineering/apps/todo-app/todo_handler.c`

- [ ] **Step 1: 添加日历路由处理函数**

```c
static void handle_calendar_day(SOCKET client, const char *query_str) {
    const char *date_str = get_query_param(query_str, "date");
    const char *ts_str = get_query_param(query_str, "task_system_id");
    int64_t date = date_str ? atoll(date_str) : time(NULL);
    int64_t ts_id = ts_str ? atoll(ts_str) : -1;

    todo_t *todos = NULL;
    int count = 0;
    calendar_day_todos(date, ts_id, &todos, &count);

    cJSON *jitems = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
        cJSON_AddItemToArray(jitems, todo_to_json(&todos[i]));
    free(todos);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "date", date);
    cJSON_AddItemToObject(data, "tasks", jitems);
    send_success(client, data);
}

/* handle_calendar_week 和 handle_calendar_month 同上模式 */
```

- [ ] **Step 2: 添加顺延处理函数**

```c
static void handle_carryover_confirm(SOCKET client, const char *body) {
    // 解析 body: [{ "todo_id": 1, "completed": 1 }, ...]
    // 调用 calendar_confirm_carryover
    // 返回 carryover_report_t
}

static void handle_pending_carryover(SOCKET client) {
    todo_t *todos = NULL;
    int count = 0;
    calendar_pending_carryover(&todos, &count);
    cJSON *jitems = cJSON_CreateArray();
    for (int i = 0; i < count; i++)
        cJSON_AddItemToArray(jitems, todo_to_json(&todos[i]));
    free(todos);
    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "items", jitems);
    send_success(client, data);
}

static void handle_postpone(SOCKET client, int64_t todo_id, const char *body) {
    // 解析 { "days": 1 }
    // 调用 calendar_postpone
}

static void handle_promote(SOCKET client, const char *body) {
    // 解析 { "scope": 1 }
    // 调用 calendar_promote
}
```

- [ ] **Step 3: 在 handle_request 路由表中注册**

```c
    /* GET /api/calendar/day */
    else if (strcmp(path, "/api/calendar/day") == 0 && strcmp(method, "GET") == 0) {
        handle_calendar_day(client, query);
        handled = 1;
    }
    /* GET /api/calendar/week */
    else if (strcmp(path, "/api/calendar/week") == 0 && strcmp(method, "GET") == 0) {
        handle_calendar_week(client, query);
        handled = 1;
    }
    /* GET /api/calendar/month */
    else if (strcmp(path, "/api/calendar/month") == 0 && strcmp(method, "GET") == 0) {
        handle_calendar_month(client, query);
        handled = 1;
    }
    /* GET /api/calendar/pending-carryover */
    else if (strcmp(path, "/api/calendar/pending-carryover") == 0 && strcmp(method, "GET") == 0) {
        handle_pending_carryover(client);
        handled = 1;
    }
    /* POST /api/calendar/carryover-confirm */
    else if (strcmp(path, "/api/calendar/carryover-confirm") == 0 && strcmp(method, "POST") == 0) {
        handle_carryover_confirm(client, body);
        handled = 1;
    }
    /* POST /api/calendar/postpone/:id */
    else if (sscanf(path, "/api/calendar/postpone/%lld", ...) == 1) {
        handle_postpone(client, id_a, body);
        handled = 1;
    }
    /* POST /api/calendar/promote */
    else if (strcmp(path, "/api/calendar/promote") == 0 && strcmp(method, "POST") == 0) {
        handle_promote(client, body);
        handled = 1;
    }
```

- [ ] **Step 4: 编译 + 提交**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
git add engineering/apps/todo-app/
git commit -m "feat: 日历和顺延 API 路由"
git push
```

### Task 2.4: 实现 DFX 统计 API

**Files:**
- Modify: `engineering/apps/todo-app/todo_stats.c`
- Modify: `engineering/apps/todo-app/todo_handler.c`

- [ ] **Step 1: 实现 stats_calculate**

```c
int stats_calculate(dfx_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    int64_t now = time(NULL);
    int64_t today = today_start();

    // 遍历所有 closed 的 todo
    for (int i = 0; i < g_todo_count; i++) {
        todo_t *t = &g_todos[i];
        if (strcmp(t->status, "closed") != 0) continue;
        stats->total_completed++;

        if (t->completed_at > 0) {
            if (t->completed_at <= t->due_date) {
                stats->on_time_completed++;
                if (t->completed_at < t->due_date - 86400 * 0.5) {
                    // 提前超过半天才算提前完成
                    stats->early_completed++;
                    stats->avg_early_days += (double)(t->due_date - t->completed_at) / 86400.0;
                }
            }
        }

        if (t->carryover_count > 0) {
            stats->carried_over_count++;
            stats->avg_carryover_days += (double)t->carryover_count;
            if (t->carryover_count >= 2) stats->hard_task_ratio += 1.0;
        }
    }

    // 计算平均值
    if (stats->early_completed > 0)
        stats->avg_early_days /= stats->early_completed;
    if (stats->carried_over_count > 0)
        stats->avg_carryover_days /= stats->carried_over_count;

    // 逾期未完成
    stats->overdue_count = 0;
    for (int i = 0; i < g_todo_count; i++) {
        if (strcmp(g_todos[i].status, "open") == 0 && 
            g_todos[i].due_date > 0 && g_todos[i].due_date < today) {
            stats->overdue_count++;
        }
    }

    // 完成率
    int active = stats->total_completed + stats->overdue_count;
    stats->completion_rate = active > 0 ? (double)stats->total_completed / active : 0.0;

    // 连续完成天数
    stats->streak_days = 0;
    int64_t check_date = today - 86400;
    while (1) {
        int found = 0;
        for (int i = 0; i < g_todo_count; i++) {
            if (strcmp(g_todos[i].status, "closed") == 0 &&
                g_todos[i].completed_at >= check_date &&
                g_todos[i].completed_at < check_date + 86400) {
                found = 1; break;
            }
        }
        if (!found) break;
        stats->streak_days++;
        check_date -= 86400;
    }

    // 周趋势（近 12 周）
    // ... 略，按周统计 completed_count

    // 计划 vs 临时任务
    // ... 按 plan_id > 0 区分

    // 计划健康度
    double completion_weight = 0.5;
    double streak_weight = 0.2;
    double carryover_penalty = stats->carried_over_count > 0 ? 
        0.3 * (1.0 - (double)stats->carried_over_count / (double)stats->total_completed) : 0.0;
    carryover_penalty = carryover_penalty > 0 ? carryover_penalty : 0.0;
    stats->plan_health_score = (stats->completion_rate * completion_weight + 
        (double)stats->streak_days / 30.0 * streak_weight + carryover_penalty) * 100.0;
    if (stats->plan_health_score > 100.0) stats->plan_health_score = 100.0;

    return 0;
}
```

- [ ] **Step 2: 实现 stats_calculate_heatmap**

```c
int stats_calculate_heatmap(heatmap_data_t *heatmap) {
    memset(heatmap, 0, sizeof(*heatmap));
    int64_t today = today_start();
    // 计算一年前的日期
    struct tm *t = localtime((time_t *)&today);
    struct tm past = *t;
    past.tm_year -= 1;
    int64_t one_year_ago = (int64_t)mktime(&past);

    // 从一年前开始，按周分组
    int64_t ws = week_start(one_year_ago);
    int max_count = 0;
    heatmap->weeks_count = 0;

    while (ws < today && heatmap->weeks_count < 53) {
        heatmap_week_t *week = &heatmap->weeks[heatmap->weeks_count];
        week->week_start = ws;
        for (int d = 0; d < 7; d++) {
            int64_t day_ts = ws + d * 86400;
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

    // 计算色级
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
```

- [ ] **Step 3: 在 todo_handler.c 中添加 DFX 统计路由**

```c
/* GET /api/stats-dfx */
static void handle_stats_dfx(SOCKET client) {
    dfx_stats_t stats;
    stats_calculate(&stats);
    // 序列化 stats 为 JSON 返回
}

/* GET /api/stats-dfx/heatmap */
static void handle_stats_heatmap(SOCKET client) {
    heatmap_data_t heatmap;
    stats_calculate_heatmap(&heatmap);
    // 序列化 heatmap 为 JSON 返回
}
```

- [ ] **Step 4: 编译 + 提交**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
git add engineering/apps/todo-app/
git commit -m "feat: DFX 统计 + 热力图 API"
git push
```

---

## P3：学习计划种子数据 + 计划展开

### Task 3.1: 实现 plan_import_template

**Files:**
- Modify: `engineering/apps/todo-app/todo_plan.c`
- Modify: `engineering/apps/todo-app/todo_plan.h`

- [ ] **Step 1: 定义预置学习计划的数据结构**

```c
/* 计划项模板（用于静态初始化） */
typedef struct {
    const char *title;
    int         item_type;       /* 1=阶段 2=模块 3=任务 */
    int         parent_index;    /* 父项在数组中的索引，-1=根 */
    int         estimated_minutes;
    int         completion_rule; /* 1=按日 2=按周 3=按月 */
    int         days_offset;     /* 距离计划起始日的偏移天数 */
} plan_template_item_t;
```

- [ ] **Step 2: 编码关系型数据库完全学习计划模板**

```c
static const char *PLAN_NAME = "关系型数据库完全学习计划";
static const char *PLAN_DESC = 
    "学习路径：Phase 1 通用架构（2周）→ Phase 2 SQLite 源码（2个月）→ Phase 3 PostgreSQL 源码（6个月）\n\n"
    "Phase 1: 数据库通用架构\n"
    "  目标：建立全局心智模型，理解所有关系型数据库的通用子系统\n"
    "  推荐资料：Database Internals by Alex Petrov、CMU 15445 Andy Pavlo 课程\n"
    "  产出：存储架构图 + 查询执行全流程图\n\n"
    "Phase 2: SQLite 源码精读\n"
    "  目标：理解最小完备实现的所有子系统\n"
    "  推荐资料：《SQLite 源码分析》林志强、sqlite.org/arch.html\n"
    "  产出：8 篇学习笔记到 learning/notes/sqlite/\n\n"
    "Phase 3: PostgreSQL 源码深入\n"
    "  目标：啃透核心子系统的标杆实现，在自己的 PG 风格项目上验证\n"
    "  推荐资料：《PostgreSQL 内核分析》陈昌男、PG 官方文档 Part VII Internals\n"
    "  产出：在 engineering/src/db/ 上验证各子系统";

static const plan_template_item_t PLAN_ITEMS[] = {
    /* Phase 1: 数据库通用架构 */
    { "Phase 1: 数据库通用架构", 1, -1, 0, 3, 0 },
    { "Week 1: 存储引擎", 2, 0, 0, 2, 0 },
    { "读 Database Internals Ch.1-2 存储引擎", 3, 1, 60, 1, 0 },
    { "理解 B-Tree / LSM-Tree 原理", 3, 1, 45, 1, 1 },
    { "理解 Buffer Pool / Page Management", 3, 1, 45, 1, 2 },
    { "画出通用存储架构图", 3, 1, 30, 1, 3 },
    { "Week 2: 查询处理与事务", 2, 0, 0, 2, 7 },
    { "读 Database Internals Ch.3-5 查询处理", 3, 5, 60, 1, 7 },
    { "理解 Parser → Planner → Executor 管道", 3, 5, 45, 1, 8 },
    { "理解 ACID / MVCC / WAL / 隔离级别", 3, 5, 60, 1, 9 },
    { "画出查询执行全流程图", 3, 5, 30, 1, 10 },
    { "Phase 1 总结笔记", 3, 5, 30, 1, 11 },

    /* Phase 2: SQLite 源码精读 */
    { "Phase 2: SQLite 源码精读", 1, -1, 0, 3, 14 },
    { "W3: 全局骨架 + VDBE 概览", 2, 12, 0, 2, 14 },
    { "阅读 SQLite 架构文档", 3, 13, 45, 1, 14 },
    { "Trace SQL → VDBE 字节码的全路径", 3, 13, 60, 1, 15 },
    { "理解 VDBE 虚拟机概览", 3, 13, 45, 1, 16 },
    { "W4: Tokenizer + Parser", 2, 12, 0, 2, 21 },
    { "阅读 tokenize.c 源码", 3, 17, 60, 1, 21 },
    { "理解 parse.y LALR 文法", 3, 17, 60, 1, 22 },
    { "Lemon 解析器生成流程", 3, 17, 45, 1, 23 },
    { "W5: Code Generator", 2, 12, 0, 2, 28 },
    { "阅读 build.c 源码", 3, 20, 60, 1, 28 },
    { "理解 SQL → VDBE 指令序列", 3, 20, 60, 1, 29 },
    { "W6: VDBE 虚拟机", 2, 12, 0, 2, 35 },
    { "精读 vdbe.c 核心循环", 3, 23, 90, 1, 35 },
    { "理解游标操作和指令执行", 3, 23, 60, 1, 36 },
    { "W7: B-Tree 存储层", 2, 12, 0, 2, 42 },
    { "精读 btree.c 核心", 3, 26, 90, 1, 42 },
    { "理解页面分裂和合并", 3, 26, 60, 1, 43 },
    { "W8: Pager 层", 2, 12, 0, 2, 49 },
    { "精读 pager.c 核心", 3, 29, 90, 1, 49 },
    { "理解事务提交和回滚", 3, 29, 60, 1, 50 },
    { "W9: 事务 + 锁", 2, 12, 0, 2, 56 },
    { "理解 5 级锁协议", 3, 32, 60, 1, 56 },
    { "理解原子提交", 3, 32, 45, 1, 57 },
    { "W10: 回看 + 串联", 2, 12, 0, 2, 63 },
    { "从 prepare 到 step 完整路径", 3, 35, 90, 1, 63 },
    { "写 SQLite 架构总结笔记", 3, 35, 45, 1, 64 },

    /* Phase 3: PostgreSQL 源码深入 */
    { "Phase 3: PostgreSQL 源码深入", 1, -1, 0, 3, 70 },
    { "M1: 存储引擎 + Buffer Pool", 2, 38, 0, 3, 70 },
    { "精读 PG bufmgr.c", 3, 39, 90, 1, 70 },
    { "理解 smgr + md 层", 3, 39, 60, 1, 72 },
    { "对照你的 bufmgr 实现补全", 3, 39, 60, 1, 74 },
    { "M2: Heap + 元组系统", 2, 38, 0, 3, 84 },
    { "精读 PG heapam.c", 3, 43, 90, 1, 84 },
    { "理解 tqual.c 可见性判断", 3, 43, 60, 1, 86 },
    { "在你项目上实现可见性", 3, 43, 60, 1, 88 },
    { "M3: BTree 索引", 2, 38, 0, 3, 98 },
    { "精读 nbtinsert / nbtsearch", 3, 47, 90, 1, 98 },
    { "理解页面分裂逻辑", 3, 47, 60, 1, 100 },
    { "补完你的 btree 分裂逻辑", 3, 47, 60, 1, 102 },
    { "M4: WAL + 检查点 + 恢复", 2, 38, 0, 3, 112 },
    { "精读 PG xlog.c", 3, 51, 90, 1, 112 },
    { "理解检查点和恢复流程", 3, 51, 60, 1, 114 },
    { "在你的项目上加 REDO", 3, 51, 60, 1, 116 },
    { "M5: 查询优化器", 2, 38, 0, 3, 126 },
    { "精读 PG planner.c", 3, 55, 90, 1, 126 },
    { "理解 costsize 代价估计", 3, 55, 60, 1, 128 },
    { "在你的 planner 上加真实代价", 3, 55, 60, 1, 130 },
    { "M6: 执行器", 2, 38, 0, 3, 140 },
    { "精读 execMain + execProcnode", 3, 59, 90, 1, 140 },
    { "与你的 ExecHashJoin 等对比", 3, 59, 60, 1, 142 },
    { "写 PG 架构总结笔记", 3, 59, 45, 1, 144 },
};

static const int PLAN_ITEM_COUNT = sizeof(PLAN_ITEMS) / sizeof(PLAN_ITEMS[0]);
```

- [ ] **Step 3: 实现 plan_import_template**

```c
int plan_import_template(int template_id, int64_t start_date, int64_t task_system_id) {
    if (template_id != TEMPLATE_DB_LEARNING) return -1;
    if (task_system_id <= 0) return -1;

    // 1. 创建 plan
    plan_t plan;
    memset(&plan, 0, sizeof(plan));
    strncpy(plan.name, PLAN_NAME, PLAN_NAME_MAX - 1);
    strncpy(plan.description, PLAN_DESC, PLAN_DESC_MAX - 1);
    plan.start_date = start_date;
    plan.end_date = start_date + 300 * 86400; /* ~10 个月 */
    strcpy(plan.color, "#4A90D9");
    plan.status = 0;

    int64_t plan_id;
    if (plan_create(&plan, &plan_id) != 0) return -1;

    // 2. 创建 plan_items，计算每个任务的实际日期
    for (int i = 0; i < PLAN_ITEM_COUNT; i++) {
        plan_item_t item;
        memset(&item, 0, sizeof(item));
        item.plan_id = plan_id;
        item.parent_id = PLAN_ITEMS[i].parent_index >= 0 ? 
            /* 父项 ID 需要从已创建的 items 中获取 */ 0 : 0;
        strncpy(item.title, PLAN_ITEMS[i].title, ITEM_TITLE_MAX - 1);
        item.item_type = PLAN_ITEMS[i].item_type;
        item.estimated_minutes = PLAN_ITEMS[i].estimated_minutes;
        item.completion_rule = PLAN_ITEMS[i].completion_rule;
        item.planned_date = start_date + (int64_t)PLAN_ITEMS[i].days_offset * 86400;
        item.order_index = i;

        int64_t item_id;
        plan_item_create(&item, &item_id);
    }

    return 0;
}
```

- [ ] **Step 4: 实现 plan_expand_to_todos**

```c
int plan_expand_to_todos(int64_t plan_id, int64_t task_system_id) {
    // 1. 获取该 plan 的所有 item
    plan_item_t *items = NULL;
    int count = 0;
    plan_item_list_by_plan(plan_id, &items, &count);
    if (count == 0) return -1;

    // 2. 只展开 type=3（具体任务）的 item
    for (int i = 0; i < count; i++) {
        if (items[i].item_type != 3 || items[i].planned_date == 0) continue;
        if (items[i].todo_id > 0) continue; /* 已展开过，跳过 */

        todo_t todo;
        memset(&todo, 0, sizeof(todo));
        strncpy(todo.title, items[i].title, TODO_TITLE_MAX - 1);
        todo.due_date = items[i].planned_date;
        todo.original_date = items[i].planned_date;
        todo.priority = PRIORITY_MEDIUM;
        todo.todo_type = 0;
        todo.task_system_id = task_system_id;
        todo.plan_id = plan_id;
        todo.plan_item_id = items[i].id;

        int64_t todo_id;
        if (todo_create(&todo, &todo_id) == 0) {
            // 回写 todo_id 到 plan_item
            items[i].todo_id = todo_id;
            plan_item_update(&items[i]);
        }
    }

    plan_item_list_free(items, count);
    return 0;
}
```

- [ ] **Step 5: 在 todo_handler.c 中添加计划 API 路由**

```c
/* POST /api/plans/import-template */
static void handle_plan_import_template(SOCKET client, const char *body) {
    // 解析 { "template_id": 1, "start_date": 1721491200, "task_system_id": 1 }
    // 调用 plan_import_template
}

/* POST /api/plans/:id/expand */
static void handle_plan_expand(SOCKET client, int64_t plan_id, const char *body) {
    // 解析 { "task_system_id": 1 }
    // 调用 plan_expand_to_todos
}

/* GET /api/plans */
/* GET /api/plans/:id */
/* POST /api/plans */
/* PATCH /api/plans/:id */
/* DELETE /api/plans/:id */
// 以上 plan CRUD 路由模式同 task_system
```

- [ ] **Step 6: 编译 + 提交**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -5
git add engineering/apps/todo-app/
git commit -m "feat: 预置关系型数据库学习计划 + 计划展开 API"
git push
```

---

## P4：前端日历三视图

### Task 4.1: 扩展前端路由和 API

**Files:**
- Modify: `engineering/apps/web/todo-app/src/router/index.js`
- Modify: `engineering/apps/web/todo-app/src/api.js`
- Modify: `engineering/apps/web/todo-app/src/App.vue`

- [ ] **Step 1: 扩展 api.js**

```javascript
const api = {
  // ... 现有方法不变 ...

  /* 日历 */
  calendarDay: (date, tsId) => fetch(`${BASE}/calendar/day?date=${date}&task_system_id=${tsId||-1}`).then(r => r.json()),
  calendarWeek: (date, tsId) => fetch(`${BASE}/calendar/week?date=${date}&task_system_id=${tsId||-1}`).then(r => r.json()),
  calendarMonth: (date, tsId) => fetch(`${BASE}/calendar/month?date=${date}&task_system_id=${tsId||-1}`).then(r => r.json()),
  pendingCarryover: () => fetch(`${BASE}/calendar/pending-carryover`).then(r => r.json()),
  confirmCarryover: (items) => fetch(`${BASE}/calendar/carryover-confirm`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(items) }).then(r => r.json()),

  /* DFX 统计 */
  statsDfx: () => fetch(`${BASE}/stats-dfx`).then(r => r.json()),
  statsHeatmap: () => fetch(`${BASE}/stats-dfx/heatmap`).then(r => r.json()),

  /* 任务系统 */
  listTaskSystems: () => fetch(`${BASE}/task-systems`).then(r => r.json()),
  createTaskSystem: (body) => fetch(`${BASE}/task-systems`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  updateTaskSystem: (id, body) => fetch(`${BASE}/task-systems/${id}`, { method: 'PATCH', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  deleteTaskSystem: (id) => fetch(`${BASE}/task-systems/${id}`, { method: 'DELETE' }).then(r => r.json()),

  /* 学习计划 */
  listPlans: () => fetch(`${BASE}/plans`).then(r => r.json()),
  getPlan: (id) => fetch(`${BASE}/plans/${id}`).then(r => r.json()),
  createPlan: (body) => fetch(`${BASE}/plans`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  importTemplate: (body) => fetch(`${BASE}/plans/import-template`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
  expandPlan: (id, body) => fetch(`${BASE}/plans/${id}/expand`, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) }).then(r => r.json()),
}
```

- [ ] **Step 2: 扩展路由**

```javascript
import CalendarView from '../views/CalendarView.vue'
import StatsDFXView from '../views/StatsDFXView.vue'
import PlanManageView from '../views/PlanManageView.vue'

const routes = [
  // ... 现有路由不变 ...
  { path: '/calendar', component: CalendarView, meta: { title: '日历' } },
  { path: '/stats-dfx', component: StatsDFXView, meta: { title: 'DFX 统计' } },
  { path: '/plan-manage', component: PlanManageView, meta: { title: '计划管理' } },
]
```

- [ ] **Step 3: 在 App.vue 导航栏添加新链接**

```html
<router-link to="/calendar" class="nav-link">📅 日历</router-link>
<router-link to="/stats-dfx" class="nav-link">📊 DFX</router-link>
<router-link to="/plan-manage" class="nav-link">📋 计划</router-link>
```

- [ ] **Step 4: 提交**

```bash
git add engineering/apps/web/todo-app/src/
git commit -m "feat: 前端路由扩展 + API 新增日历/统计/计划接口"
git push
```

### Task 4.2: 实现 CalendarView 月视图

**Files:**
- Create: `engineering/apps/web/todo-app/src/views/CalendarView.vue`

- [ ] **Step 1: 创建 CalendarView.vue**

```html
<template>
  <div class="calendar-page">
    <div class="calendar-header">
      <button @click="prevMonth">◀</button>
      <h2>{{ currentYear }} 年 {{ currentMonth + 1 }} 月</h2>
      <button @click="nextMonth">▶</button>
      <div class="view-toggle">
        <button :class="{ active: view === 'month' }" @click="view = 'month'">月</button>
        <button :class="{ active: view === 'week' }" @click="view = 'week'">周</button>
        <button :class="{ active: view === 'day' }" @click="view = 'day'">日</button>
      </div>
    </div>
    <div v-if="view === 'month'" class="month-grid">
      <div class="weekday-header">
        <div v-for="d in ['周一', '周二', '周三', '周四', '周五', '周六', '周日']" :key="d">{{ d }}</div>
      </div>
      <div class="days-grid">
        <div v-for="(day, idx) in monthDays" :key="idx"
             :class="['day-cell', { today: day.isToday, 'other-month': !day.isCurrentMonth, 'has-task': day.tasks.length > 0, 'weekend': day.isWeekend }]"
             @click="selectDay(day.date)">
          <span class="day-number">{{ day.dayNum }}</span>
          <div class="day-tasks">
            <div v-for="task in day.tasks.slice(0, 3)" :key="task.id" class="mini-task"
                 :style="{ borderLeftColor: priorityColor(task.priority) }">
              ☐ {{ task.title }}
            </div>
            <div v-if="day.tasks.length > 3" class="more-tasks">+{{ day.tasks.length - 3 }} 更多</div>
          </div>
        </div>
      </div>
    </div>
    <div v-else-if="view === 'week'" class="week-view">
      <!-- 周视图：7列格子，每列按优先级堆叠任务 -->
    </div>
    <div v-else class="day-view">
      <!-- 日视图：当日任务列表 -->
    </div>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, watch } from 'vue'
import api from '../api.js'

const view = ref('month')
const currentDate = ref(new Date())
const tasksByDate = ref({})

const currentYear = computed(() => currentDate.value.getFullYear())
const currentMonth = computed(() => currentDate.value.getMonth())

// 加载当月数据
async function loadMonth() {
  const ts = Math.floor(currentDate.value.getTime() / 1000)
  const res = await api.calendarMonth(ts, -1)
  if (res.code === 0 && res.data) {
    // 按日期索引任务
    tasksByDate.value = {}
    for (const task of res.data.tasks) {
      const dateKey = new Date(task.due_date * 1000).toDateString()
      if (!tasksByDate.value[dateKey]) tasksByDate.value[dateKey] = []
      tasksByDate.value[dateKey].push(task)
    }
  }
}

// 计算当月所有日期格子
const monthDays = computed(() => {
  const year = currentYear.value
  const month = currentMonth.value
  const firstDay = new Date(year, month, 1)
  // 这个月第一天是周几（1=周一）
  let startDow = firstDay.getDay()
  if (startDow === 0) startDow = 7
  const days = []
  // 填充上个月空缺
  const prevMonthDays = new Date(year, month, 0).getDate()
  for (let i = startDow - 1; i > 0; i--) {
    days.push({ dayNum: prevMonthDays - i + 1, isCurrentMonth: false, isToday: false, tasks: [], date: null })
  }
  // 本月
  const totalDays = new Date(year, month + 1, 0).getDate()
  const today = new Date()
  for (let d = 1; d <= totalDays; d++) {
    const date = new Date(year, month, d)
    const dateKey = date.toDateString()
    days.push({
      dayNum: d,
      isCurrentMonth: true,
      isToday: date.toDateString() === today.toDateString(),
      isWeekend: date.getDay() === 0 || date.getDay() === 6,
      tasks: tasksByDate.value[dateKey] || [],
      date: date
    })
  }
  return days
})

function prevMonth() {
  currentDate.value = new Date(currentYear.value, currentMonth.value - 1, 1)
}
function nextMonth() {
  currentDate.value = new Date(currentYear.value, currentMonth.value + 1, 1)
}
function selectDay(date) {
  if (date) { currentDate.value = date; view.value = 'day' }
}
function priorityColor(p) {
  return ['#E53935', '#FB8C00', '#FFB300', '#42A5F5', '#9E9E9E'][p] || '#9E9E9E'
}

onMounted(loadMonth)
watch(currentDate, loadMonth)
</script>

<style scoped>
/* Outlook 风格样式 */
.calendar-page { padding: 16px; }
.calendar-header { display: flex; align-items: center; gap: 12px; margin-bottom: 16px; }
.calendar-header h2 { flex: 1; font-size: 1.3em; }
.view-toggle button { padding: 4px 12px; border: 1px solid #ddd; background: #fff; cursor: pointer; }
.view-toggle button.active { background: #0078D4; color: #fff; border-color: #0078D4; }
.weekday-header { display: grid; grid-template-columns: repeat(7, 1fr); text-align: center; font-weight: bold; color: #666; padding: 8px 0; }
.days-grid { display: grid; grid-template-columns: repeat(7, 1fr); gap: 1px; }
.day-cell { min-height: 80px; border: 1px solid #eee; padding: 4px; cursor: pointer; }
.day-cell.today .day-number { background: #0078D4; color: #fff; border-radius: 50%; width: 24px; height: 24px; display: inline-flex; align-items: center; justify-content: center; }
.day-cell.other-month { opacity: 0.4; }
.day-cell.weekend { background: #fafafa; }
.day-number { font-size: 0.85em; margin-bottom: 4px; }
.mini-task { font-size: 0.75em; padding: 1px 4px; border-left: 3px solid; margin-bottom: 2px; cursor: pointer; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.mini-task:hover { background: #f0f0f0; }
.more-tasks { font-size: 0.75em; color: #0078D4; cursor: pointer; }
</style>
```

- [ ] **Step 2: 提交**

```bash
git add engineering/apps/web/todo-app/src/
git commit -m "feat: 日历月视图 + 周视图骨架 + 日视图骨架"
git push
```

---

## P5：前端过期任务弹窗 + DFX 页面 + 热力图

### Task 5.1: CarryoverModal 组件

**Files:**
- Create: `engineering/apps/web/todo-app/src/components/CarryoverModal.vue`

- [ ] **Step 1: 创建 CarryoverModal.vue**

```html
<template>
  <div class="modal-overlay" v-if="show" @click.self="$emit('close')">
    <div class="modal">
      <div class="modal-header">
        <h3>📋 过期任务确认</h3>
        <button class="close-btn" @click="$emit('close')">✕</button>
      </div>
      <p>您有 {{ tasks.length }} 个任务已过期，请确认是否已完成：</p>
      <div class="task-list">
        <div v-for="task in tasks" :key="task.id" class="task-item">
          <label>
            <input type="checkbox" v-model="task.completed" />
            <span class="task-title">{{ task.title }}</span>
            <span class="task-date">预期：{{ formatDate(task.due_date) }}</span>
          </label>
        </div>
      </div>
      <div class="modal-actions">
        <button class="btn btn-secondary" @click="markAllDone">全部标记已完成</button>
        <button class="btn btn-primary" @click="confirm">确认</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, watch } from 'vue'

const props = defineProps({ show: Boolean, tasks: Array })
const emit = defineEmits(['close', 'confirm'])

// 本地状态：每项追加 completed 标记
const localTasks = ref([])
watch(() => props.tasks, (t) => {
  localTasks.value = (t || []).map(t => ({ ...t, completed: false }))
}, { immediate: true })

function markAllDone() {
  localTasks.value.forEach(t => t.completed = true)
}

function confirm() {
  emit('confirm', localTasks.value.map(t => ({ todo_id: t.id, completed: t.completed ? 1 : 0 })))
}

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getMonth() + 1}/${d.getDate()}`
}
</script>

<style scoped>
.modal-overlay { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.4); display: flex; align-items: center; justify-content: center; z-index: 1000; }
.modal { background: #fff; border-radius: 8px; padding: 24px; min-width: 480px; max-height: 80vh; overflow-y: auto; }
.modal-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 16px; }
.task-list { margin: 16px 0; }
.task-item { padding: 8px 0; border-bottom: 1px solid #eee; }
.task-item label { display: flex; align-items: center; gap: 8px; cursor: pointer; }
.task-title { flex: 1; }
.task-date { color: #999; font-size: 0.85em; }
.modal-actions { display: flex; gap: 8px; justify-content: flex-end; }
</style>
```

- [ ] **Step 2: 在 App.vue 中集成 CarryoverModal**

```javascript
import CarryoverModal from './components/CarryoverModal.vue'
import { onMounted, ref } from 'vue'
import api from './api.js'

const showCarryover = ref(false)
const carryoverTasks = ref([])

async function checkCarryover() {
  const res = await api.pendingCarryover()
  if (res.code === 0 && res.data && res.data.items && res.data.items.length > 0) {
    carryoverTasks.value = res.data.items
    showCarryover.value = true
  }
}

async function handleCarryoverConfirm(items) {
  await api.confirmCarryover(items)
  showCarryover.value = false
  showToast('过期任务处理完成')
}

onMounted(() => {
  checkCarryover()
})
```

- [ ] **Step 3: 提交**

```bash
git add engineering/apps/web/todo-app/src/
git commit -m "feat: 过期任务批量确认弹窗组件"
git push
```

### Task 5.2: StatsDFXView 页面 + HeatmapChart 组件

**Files:**
- Create: `engineering/apps/web/todo-app/src/views/StatsDFXView.vue`
- Create: `engineering/apps/web/todo-app/src/components/HeatmapChart.vue`

- [ ] **Step 1: 创建 StatsDFXView.vue**

```html
<template>
  <div class="stats-dfx-page">
    <h2>DFX 统计</h2>
    <div class="summary-cards">
      <div class="card"><div class="card-value">{{ stats.total_completed }}</div><div class="card-label">总完成</div></div>
      <div class="card success"><div class="card-value">{{ stats.on_time_completed }}</div><div class="card-label">按时完成</div></div>
      <div class="card info"><div class="card-value">{{ stats.early_completed }}</div><div class="card-label">提前完成</div></div>
      <div class="card warning"><div class="card-value">{{ stats.carried_over_count }}</div><div class="card-label">顺延次数</div></div>
      <div class="card danger"><div class="card-value">{{ stats.overdue_count }}</div><div class="card-label">逾期未完成</div></div>
    </div>
    <div class="stats-row">
      <div class="stat-block">
        <span class="stat-label">完成率</span>
        <span class="stat-value">{{ (stats.completion_rate * 100).toFixed(1) }}%</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">连续完成</span>
        <span class="stat-value">{{ stats.streak_days }} 天</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">计划健康度</span>
        <span class="stat-value">{{ stats.plan_health_score.toFixed(0) }} 分</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">平均提前</span>
        <span class="stat-value">{{ stats.avg_early_days.toFixed(1) }} 天</span>
      </div>
      <div class="stat-block">
        <span class="stat-label">平均顺延</span>
        <span class="stat-value">{{ stats.avg_carryover_days.toFixed(1) }} 次</span>
      </div>
    </div>
    <HeatmapChart :data="heatmap" />
    <div class="weekly-trend">
      <h3>周完成趋势</h3>
      <div class="trend-bars">
        <div v-for="(w, i) in stats.weekly_trend" :key="i" class="trend-bar" :style="{ height: barHeight(w.completed_count) + 'px' }">
          <span class="bar-value">{{ w.completed_count }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted } from 'vue'
import api from '../api.js'
import HeatmapChart from '../components/HeatmapChart.vue'

const stats = ref({ total_completed: 0, on_time_completed: 0, early_completed: 0, carried_over_count: 0, overdue_count: 0, completion_rate: 0, streak_days: 0, plan_health_score: 0, avg_early_days: 0, avg_carryover_days: 0, weekly_trend: [] })
const heatmap = ref({ weeks: [], max_daily_count: 0 })

async function load() {
  const r1 = await api.statsDfx()
  if (r1.code === 0) stats.value = r1.data
  const r2 = await api.statsHeatmap()
  if (r2.code === 0) heatmap.value = r2.data
}

function barHeight(v) { return Math.max(4, v * 20) }

onMounted(load)
</script>

<style scoped>
.stats-dfx-page { padding: 16px; }
.summary-cards { display: grid; grid-template-columns: repeat(5, 1fr); gap: 12px; margin-bottom: 24px; }
.card { background: #fff; border-radius: 8px; padding: 16px; text-align: center; box-shadow: 0 1px 3px rgba(0,0,0,0.1); }
.card-value { font-size: 2em; font-weight: bold; }
.card-label { color: #666; font-size: 0.85em; margin-top: 4px; }
.card.success .card-value { color: #4CAF50; }
.card.info .card-value { color: #2196F3; }
.card.warning .card-value { color: #FF9800; }
.card.danger .card-value { color: #E53935; }
.stats-row { display: flex; gap: 24px; margin-bottom: 24px; flex-wrap: wrap; }
.stat-block { text-align: center; }
.stat-label { display: block; font-size: 0.85em; color: #666; }
.stat-value { font-size: 1.2em; font-weight: bold; }
.weekly-trend { margin-top: 24px; }
.trend-bars { display: flex; gap: 4px; align-items: flex-end; height: 120px; }
.trend-bar { background: #0078D4; width: 24px; border-radius: 4px 4px 0 0; position: relative; }
.bar-value { position: absolute; top: -18px; left: 50%; transform: translateX(-50%); font-size: 0.75em; }
</style>
```

- [ ] **Step 2: 创建 HeatmapChart.vue**

```html
<template>
  <div class="heatmap">
    <h3>学习热力图</h3>
    <div class="heatmap-grid">
      <div class="month-labels">
        <div v-for="(m, i) in monthLabels" :key="i">{{ m }}</div>
      </div>
      <div class="weekday-labels">
        <div v-for="d in ['一', '三', '五']" :key="d">{{ d }}</div>
      </div>
      <div class="cells">
        <div v-for="(week, wi) in data.weeks" :key="wi" class="week-column">
          <div v-for="(day, di) in week.days" :key="di"
               :class="['cell', 'level-' + day.level]"
               :title="`${formatDate(day.date)}: ${day.completed_count} 个完成`">
          </div>
        </div>
      </div>
    </div>
    <div class="heatmap-legend">
      <span>少</span>
      <div class="cell level-0"></div>
      <div class="cell level-1"></div>
      <div class="cell level-2"></div>
      <div class="cell level-3"></div>
      <span>多</span>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue'

const props = defineProps({ data: Object })

const monthLabels = computed(() => {
  const labels = []
  if (!props.data?.weeks?.length) return labels
  let lastMonth = -1
  for (const week of props.data.weeks) {
    for (const day of week.days) {
      if (!day.date) continue
      const d = new Date(day.date * 1000)
      if (d.getMonth() !== lastMonth) {
        labels.push(`${d.getMonth() + 1}月`)
        lastMonth = d.getMonth()
        break
      }
    }
  }
  return labels
})

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getFullYear()}/${d.getMonth() + 1}/${d.getDate()}`
}
</script>

<style scoped>
.heatmap { margin-top: 24px; }
.heatmap-grid { display: flex; gap: 4px; overflow-x: auto; }
.month-labels { display: flex; flex-direction: column; font-size: 0.75em; color: #666; gap: 8px; }
.weekday-labels { display: flex; flex-direction: column; justify-content: space-around; font-size: 0.75em; color: #666; padding: 0 4px; }
.cells { display: flex; gap: 2px; }
.week-column { display: flex; flex-direction: column; gap: 2px; }
.cell { width: 12px; height: 12px; border-radius: 2px; }
.level-0 { background: #ebedf0; }
.level-1 { background: #9be9a8; }
.level-2 { background: #40c463; }
.level-3 { background: #30a14e; }
.heatmap-legend { display: flex; align-items: center; gap: 4px; margin-top: 8px; font-size: 0.75em; color: #666; }
</style>
```

- [ ] **Step 3: 提交**

```bash
git add engineering/apps/web/todo-app/src/
git commit -m "feat: DFX 统计页面 + 热力图组件"
git push
```

---

## P6：前端任务系统 + 计划管理页面

### Task 6.1: PlanManageView 页面

**Files:**
- Create: `engineering/apps/web/todo-app/src/views/PlanManageView.vue`

- [ ] **Step 1: 创建 PlanManageView.vue**

```html
<template>
  <div class="plan-manage-page">
    <div class="panel">
      <div class="panel-left">
        <h3>任务系统</h3>
        <div class="system-list">
          <div v-for="ts in taskSystems" :key="ts.id"
               :class="['system-item', { active: selectedTs === ts.id }]"
               @click="selectedTs = ts.id">
            <span class="system-color" :style="{ background: ts.color }"></span>
            {{ ts.name }}
          </div>
        </div>
        <button class="btn btn-sm" @click="showCreateTs = true">+ 新建系统</button>
      </div>
      <div class="panel-right">
        <h3>学习计划</h3>
        <div class="plan-list">
          <div v-for="plan in plans" :key="plan.id" class="plan-card">
            <div class="plan-header">
              <span class="plan-name">{{ plan.name }}</span>
              <span class="plan-date">{{ formatDate(plan.start_date) }} → {{ formatDate(plan.end_date) }}</span>
            </div>
            <p class="plan-desc">{{ plan.description }}</p>
            <div class="plan-actions">
              <button class="btn btn-sm" @click="expandPlan(plan.id)">展开为 Todo</button>
            </div>
          </div>
        </div>
        <button class="btn btn-sm" @click="importTemplate">导入预置计划</button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, watch } from 'vue'
import api from '../api.js'

const selectedTs = ref(null)
const taskSystems = ref([])
const plans = ref([])

async function load() {
  const r1 = await api.listTaskSystems()
  if (r1.code === 0) {
    taskSystems.value = r1.data
    if (!selectedTs.value && r1.data.length > 0) selectedTs.value = r1.data[0].id
  }
  const r2 = await api.listPlans()
  if (r2.code === 0) plans.value = r2.data
}

async function importTemplate() {
  if (!selectedTs.value) { alert('请先选择一个任务系统'); return }
  const res = await api.importTemplate({ template_id: 1, start_date: Math.floor(Date.now() / 1000), task_system_id: selectedTs.value })
  if (res.code === 0) {
    showToast('计划导入成功')
    load()
  }
}

async function expandPlan(planId) {
  if (!selectedTs.value) { alert('请先选择一个任务系统'); return }
  const res = await api.expandPlan(planId, { task_system_id: selectedTs.value })
  if (res.code === 0) showToast('计划已展开为任务')
}

function formatDate(ts) {
  if (!ts) return ''
  const d = new Date(ts * 1000)
  return `${d.getFullYear()}-${d.getMonth() + 1}-${d.getDate()}`
}

onMounted(load)
</script>

<style scoped>
.plan-manage-page { padding: 16px; }
.panel { display: flex; gap: 24px; }
.panel-left { width: 240px; border-right: 1px solid #eee; padding-right: 16px; }
.panel-right { flex: 1; }
.system-item { padding: 8px; cursor: pointer; display: flex; align-items: center; gap: 8px; border-radius: 4px; }
.system-item:hover { background: #f0f0f0; }
.system-item.active { background: #e3f2fd; }
.system-color { width: 12px; height: 12px; border-radius: 50%; display: inline-block; }
.plan-card { background: #fff; border: 1px solid #eee; border-radius: 8px; padding: 16px; margin-bottom: 12px; }
.plan-header { display: flex; justify-content: space-between; margin-bottom: 8px; }
.plan-name { font-weight: bold; }
.plan-date { color: #999; font-size: 0.85em; }
.plan-desc { white-space: pre-wrap; color: #666; font-size: 0.9em; margin-bottom: 12px; max-height: 80px; overflow-y: auto; }
.plan-actions { display: flex; gap: 8px; }
</style>
```

- [ ] **Step 2: 提交**

```bash
git add engineering/apps/web/todo-app/src/
git commit -m "feat: 任务系统 + 计划管理页面"
git push
```

---

## P7：编译联调 + 测试 + 验证

### Task 7.1: 全量编译验证

**Files:**
- Modify: 无

- [ ] **Step 1: 编译后端**

```bash
cd build/engineering && cmake --build . --target todo-app 2>&1 | tail -20
```

预期：0 errors, 0 warnings。

- [ ] **Step 2: 编译前端**

```bash
cd engineering/apps/web/todo-app && npm run build 2>&1 | tail -20
```

预期：构建成功，生成 dist/ 目录。

- [ ] **Step 3: 启动后端验证**

```bash
cd build/engineering && ./apps/todo-app/todo-app -p 8080 -d test-calendar.json
```

- [ ] **Step 4: 导入预置计划**

```bash
curl -X POST http://localhost:8080/api/plans/import-template \
  -H 'Content-Type: application/json' \
  -d '{"template_id": 1, "start_date": 1721491200, "task_system_id": 1}'
```

预期：返回 code 0，plan 已创建。

- [ ] **Step 5: 展开计划**

```bash
curl -X POST http://localhost:8080/api/plans/1/expand \
  -H 'Content-Type: application/json' \
  -d '{"task_system_id": 1}'
```

预期：返回 code 0，所有 plan_item 展开为 todo。

- [ ] **Step 6: 验证日历 API**

```bash
curl "http://localhost:8080/api/calendar/month?date=1721491200"
```

预期：返回包含已展开任务的 JSON。

### Task 7.2: 测试迁移脚本

- [ ] **Step 1: 准备旧数据**

```bash
cp test-results/todo-app/todo_app.json test-migration.json
```

- [ ] **Step 2: 启动新版本加载旧数据**

```bash
cd build/engineering && ./apps/todo-app/todo-app -p 8081 -d test-migration.json
```

预期：启动成功，旧数据自动迁移到新格式。

### Task 7.3: 最终提交

- [ ] **Step 1: 提交所有剩余变更**

```bash
git add -A
git commit -m "feat: todo-app 日历扩展 + 学习计划注入完整实现
- 日历三视图（日/周/月）
- 智能顺延逻辑 + 后台定时任务
- 任务系统（顶层容器）
- DFX 统计 + 热力图
- 预置关系型数据库学习计划（2026-07-21 起）
- 前端路由/页面/组件扩展"
git push
```

---

## 自检清单

- [ ] 1. 设计文档覆盖：所有 spec 中的 route、数据结构、字段、API 都已在 plan 中找到对应 task
- [ ] 2. 无占位符：所有 step 都包含完整代码或具体操作
- [ ] 3. 类型一致性：前后端字段名一致（todo_type, original_date, carryover_count 等）
- [ ] 4. 学习计划种子数据：从 2026-07-21 开始排至 2027-05-20，覆盖 Phase 1-3
- [ ] 5. 现有功能保留：所有现有路由（/api/todos, /api/groups 等）和前端页面（ListView, BoardView, StatsView）不变
