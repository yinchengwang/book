# Todo App 日历与学习计划扩展 — 设计文档

> 日期：2026-07-20
> 变更：扩展现有 todo-app，增加日历视图、智能顺延、任务系统、多粒度任务、DFX 统计

---

## 1. 系统架构

```
┌──────────────────────────────────────────────────────────────┐
│                    前端 (Vue 3 + Vite)                       │
│              engineering/apps/web/todo-app/                   │
│                                                              │
│  ListView  BoardView  StatsView  CalendarView  StatsDFXView  │
│  PlanManageView                                                │
│                        ↕ fetch() / JSON                      │
├──────────────────────────────────────────────────────────────┤
│                    后端 (C + Winsock HTTP Server)             │
│              engineering/apps/todo-app/                       │
│                                                              │
│  todo_model.c     — 数据模型 + JSON 文件持久化               │
│  todo_handler.c   — REST API 路由分发                        │
│  todo_calendar.c  — 日历 + 顺延逻辑 + 后台定时任务（新增）   │
│  todo_stats.c     — DFX 统计计算（新增）                     │
│  todo_plan.c      — 学习计划展开（新增）                     │
│  todo_migration.c — 老数据迁移（扩展）                       │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. 数据模型

### 2.1 任务系统（task_systems）

每个任务系统是顶层容器，旗下管理自己的 todo 列表。

```c
typedef struct {
    int64_t id;
    char    name[64];          /* 名称，如"关系型数据库学习" */
    char    description[512];  /* 简介 */
    char    color[16];         /* 标识色 */
    int     sort_order;
    int64_t created_at;
} task_system_t;
```

### 2.2 扩展后的 todo_t

```c
typedef struct {
    /* ----- 原有字段 ----- */
    int64_t id;
    char    title[TITLE_MAX];
    char    description[DESC_MAX];
    char    status[STATUS_MAX];       /* "open" / "closed" / "archived" */
    char    labels[LABELS_MAX];       /* JSON 数组 */
    int     priority;                 /* 0=紧急 1=高 2=中 3=低 4=无 */
    int64_t due_date;                 /* 计划执行日期 */
    int64_t group_id;
    int     sort_order;
    int64_t created_at;
    int64_t updated_at;

    /* ----- 新增字段 ----- */
    int     todo_type;                /* 0=普通 1=每日 2=每周 3=每月 */
    int64_t original_date;            /* 初始排定日期（排序键） */
    int     carryover_count;          /* 累计顺延次数 */
    int64_t plan_id;                  /* 所属学习计划 */
    int64_t plan_item_id;             /* 所属计划项 */
    int64_t completed_at;             /* 完成时间 */
    int64_t postpone_until;           /* 主动延后到的目标日期 */
    int64_t task_system_id;           /* 所属任务系统（0=默认） */
} todo_t;
```

### 2.3 学习计划（plans）

```c
typedef struct {
    int64_t id;
    char    name[PLAN_NAME_MAX];      /* "关系型数据库完全学习计划" */
    char    description[DESC_MAX];    /* 计划简介（阶段+内容+资料） */
    int64_t start_date;               /* 起始日 */
    int64_t end_date;                 /* 结束日 */
    char    color[16];                /* 标识色 */
    int     status;                   /* 0=进行中 1=已完成 2=已暂停 */
    int64_t created_at;
    int64_t updated_at;
} plan_t;
```

### 2.4 计划项（plan_items）

树形结构，支持阶段→模块→具体任务三层嵌套。

```c
typedef struct {
    int64_t id;
    int64_t plan_id;
    int64_t parent_id;                /* 0=根，支持嵌套 */
    char    title[ITEM_TITLE_MAX];    /* "Phase 2：SQLite 源码精读" */
    int     item_type;                /* 1=阶段 2=模块 3=具体任务 */
    int64_t planned_date;             /* 计划日期 */
    int     estimated_minutes;        /* 预计耗时（分钟） */
    int     order_index;              /* 同级排序 */
    int     completion_rule;          /* 1=按日 2=按周 3=按月 */
    int64_t todo_id;                  /* 关联到 todo_t.id（已展开时） */
    int     actual_minutes;           /* 实际耗时（完成后填入） */
} plan_item_t;
```

### 2.5 持久化结构

```json
{
  "next_todo_id": 100,
  "next_task_system_id": 2,
  "next_plan_id": 2,
  "next_plan_item_id": 500,
  "task_systems": [
    { "id": 1, "name": "默认任务系统", "description": "", "color": "#4A90D9", "sort_order": 0 }
  ],
  "plans": [],
  "plan_items": [],
  "todos": [],
  "checklist": [],
  "groups": [],
  "comments": []
}
```

---

## 3. 顺延逻辑

### 3.1 触发时机

| 场景 | 触发条件 | 行为 |
|------|---------|------|
| 冷启动 | 服务启动时扫描所有 `due_date < 今天` 的 open 任务 | 批量弹窗，用户逐个确认"已完成/未完成" |
| 跨天 | 后台独立线程每天 00:00 触发扫描 | 批量弹窗确认 |
| 完成时 | 用户标记任务完成，`completed_at < due_date` | 自动记录为"提前完成" |

### 3.2 顺延规则

- 用户确认"未完成"→ 任务 `due_date = 今天`，`carryover_count++`
- 用户确认"已完成"→ 任务保持 `due_date`，`status = "closed"`
- 不自动跨周顺延（周内过期才自动顺延到今天）
- 跨周过期任务需手动处理

### 3.3 后台定时任务

单线程独立后台线程，不阻塞主 HTTP 服务：

```c
static HANDLE g_timer_thread = NULL;

static DWORD WINAPI timer_thread(LPVOID arg) {
    while (g_running) {
        int64_t ms_to_midnight = ms_until_tomorrow();
        Sleep((DWORD)ms_to_midnight);
        carryover_scan_and_flag();  /* 扫描过期任务，标记待确认 */
        carryover_notify_pending(); /* 通知主线程有弹窗待处理 */
    }
    return 0;
}
```

---

## 4. 排序规则

```
日历视图排序优先级：
1. original_date（初始排定日） — 主排序键
2. priority（紧急 > 高 > 中 > 低 > 无）
3. sort_order（手动拖拽顺序） — 兜底

注意：
- 顺延后 original_date 不变
- 延后操作后 original_date 不变
- 提升到本周后 original_date 不变
```

---

## 5. REST API 设计

### 5.1 日历 API

| 方法 | 路径 | 功能 |
|------|------|------|
| `GET` | `/api/calendar/day?date=2026-07-20` | 获取某天的所有任务 |
| `GET` | `/api/calendar/week?date=2026-07-20` | 获取某周的所有任务 |
| `GET` | `/api/calendar/month?date=2026-07-01` | 获取某月的所有任务 |
| `POST` | `/api/calendar/carryover-confirm` | 批量确认过期任务 |
| `GET` | `/api/calendar/pending-carryover` | 获取待确认的过期任务 |

### 5.2 任务系统 API

| 方法 | 路径 | 功能 |
|------|------|------|
| `GET` | `/api/task-systems` | 列表 |
| `POST` | `/api/task-systems` | 创建 |
| `PATCH` | `/api/task-systems/:id` | 更新 |
| `DELETE` | `/api/task-systems/:id` | 删除 |

### 5.3 计划 API

| 方法 | 路径 | 功能 |
|------|------|------|
| `GET` | `/api/plans` | 列表 |
| `POST` | `/api/plans` | 创建 |
| `GET` | `/api/plans/:id` | 详情（含树形 items） |
| `PATCH` | `/api/plans/:id` | 更新 |
| `DELETE` | `/api/plans/:id` | 删除 |
| `POST` | `/api/plans/:id/expand` | 展开为 Todo |
| `POST` | `/api/plans/import-template` | 导入预置模板 |

### 5.4 DFX 统计 API

| 方法 | 路径 | 功能 |
|------|------|------|
| `GET` | `/api/stats-dfx` | 核心指标 |
| `GET` | `/api/stats-dfx/heatmap` | 热力图数据 |

---

## 6. DFX 统计指标

### 6.1 核心指标

| 指标 | 定义 |
|------|------|
| `total_completed` | 总完成数 |
| `on_time_completed` | 按时完成数 |
| `early_completed` | 提前完成数 |
| `carried_over_count` | 顺延任务数 |
| `overdue_count` | 逾期未完成数 |
| `completion_rate` | 完成率 |
| `streak_days` | 连续完成天数 |
| `avg_early_days` | 平均提前天数 |
| `avg_carryover_days` | 平均顺延天数 |
| `weekly_trend[]` | 近8周每周完成数 |
| `velocity_ratio` | 本4周 / 前4周 完成比 |
| `plan_health_score` | 计划健康度（0-100分） |
| `phase_progress[]` | 各阶段完成百分比 |
| `estimation_accuracy` | 预估准确度 |
| `hard_task_ratio` | 困难任务（顺延≥2次）占比 |
| `plan_vs_temporary` | 计划任务 vs 临时任务完成率对比 |

### 6.2 热力图

返回过去一年的每日完成数量，按周分组，色级 0-3。

---

## 7. 前端页面

### 7.1 路由

| 路径 | 组件 | 说明 |
|------|------|------|
| `/` | `ListView.vue` | 列表视图 |
| `/board` | `BoardView.vue` | 看板视图 |
| `/stats` | `StatsView.vue` | 统计看板 |
| `/calendar` | `CalendarView.vue` | 日历视图（默认月视图） |
| `/stats-dfx` | `StatsDFXView.vue` | DFX 统计页面 |
| `/plan-manage` | `PlanManageView.vue` | 任务系统 + 计划管理 |

### 7.2 日历三视图

```
CalendarView.vue
  ├── ViewToggle.vue          — 日/周/月切换
  ├── DayView.vue             — 日视图：今日任务列表
  ├── WeekView.vue            — 周视图：7列格子，每列堆叠任务
  └── MonthView.vue           — 月视图：日历格子，超出显示 "+N more"
```

任务卡片（Outlook 简化风格）：
```
┌──────────────────────────────┐
│█☐ 读 Database Internals Ch.1│  ← 左边：优先级色条 + 复选框 + 标题
└──────────────────────────────┘
```

优先级色条：紧急=红，高=橙，中=黄，低=蓝，无=灰

### 7.3 过期任务批量确认弹窗

```
┌────────────────────────────────────────────────────────────┐
│  📋 过期任务确认                                    [X]    │
├────────────────────────────────────────────────────────────┤
│  您有 N 个任务已过期，请逐个确认是否已完成：                 │
│  ┌──────────────────────────────────────────────────────┐ │
│  │ ☐ 任务标题1                        预期：7/19       │ │
│  │ ☐ 任务标题2                        预期：7/20       │ │
│  └──────────────────────────────────────────────────────┘ │
│              [全部标记已完成]  [未完成，顺延到今天]         │
└────────────────────────────────────────────────────────────┘
```

### 7.4 DFX 页面

```
StatsDFXView.vue
  ├── SummaryCards.vue        — 按时/提前/顺延/逾期 4卡片
  ├── HeatmapChart.vue        — GitHub 风格热力图
  ├── CompletionChart.vue     — 周完成趋势折线图
  ├── StreakBadge.vue         — 连续完成天数徽章
  └── TrendAnalysis.vue       — 计划 vs 临时任务对比
```

---

## 8. 开发阶段

| 阶段 | 内容 |
|------|------|
| **P1** | 后端：数据模型扩展 + 迁移 + 任务系统 CRUD |
| **P2** | 后端：日历 API + 顺延逻辑 + 后台定时任务 + DFX API |
| **P3** | 后端：学习计划 + 一键展开为 Todo |
| **P4** | 前端：日历三视图（Day/Week/Month） |
| **P5** | 前端：过期任务批量确认弹窗 + DFX 页面 + 热力图 |
| **P6** | 前端：任务系统管理 + 计划管理页面 |
| **P7** | 联调 + 测试 + 小程序适配 |

---

## 9. 预置学习计划

系统预置"关系型数据库完全学习计划"模板（ID=1），用户可直接一键导入。

### 计划概览

- **总时长**：约 10 个月（每天 1-2 小时）
- **学习路径**：Phase 1 通用架构（2周）→ Phase 2 SQLite 源码（2个月）→ Phase 3 PostgreSQL 源码（6个月）
- **起点**：2026-07-21

详见 plan_items 数据结构（plan_id=1）。
