#include "todo_plan.h"
#include "todo_model.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

/* 外部全局变量 */
extern todo_t *g_todos;
extern int g_todo_count;
extern int find_todo_idx(int64_t id);

/* ============================================================
 * 预置学习计划模板
 * ============================================================ */

/* 计划项模板（用于静态初始化） */
typedef struct {
    const char *title;
    int         item_type;       /* 1=阶段 2=模块 3=任务 */
    int         parent_index;    /* 父项在数组中的索引，-1=根 */
    int         estimated_minutes;
    int         completion_rule; /* 1=按日 2=按周 3=按月 */
    int         days_offset;     /* 距离计划起始日的偏移天数 */
} plan_template_item_t;

static const char *PLAN_NAME = "关系型数据库完全学习计划";
static const char *PLAN_DESC =
    "学习路径：Phase 1 通用架构（2周）-> Phase 2 SQLite 源码（2个月）-> Phase 3 PostgreSQL 源码（6个月）\n\n"
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
    /* Phase 1: 数据库通用架构（第 1-14 天） */
    { "Phase 1: 数据库通用架构", 1, -1, 0, 3, 0 },
    { "Week 1: 存储引擎", 2, 0, 0, 2, 0 },
    { "读 Database Internals Ch.1-2 存储引擎", 3, 1, 60, 1, 0 },
    { "理解 B-Tree / LSM-Tree 原理", 3, 1, 45, 1, 1 },
    { "理解 Buffer Pool / Page Management", 3, 1, 45, 1, 2 },
    { "画出通用存储架构图", 3, 1, 30, 1, 3 },
    { "Week 2: 查询处理与事务", 2, 0, 0, 2, 7 },
    { "读 Database Internals Ch.3-5 查询处理", 3, 5, 60, 1, 7 },
    { "理解 Parser -> Planner -> Executor 管道", 3, 5, 45, 1, 8 },
    { "理解 ACID / MVCC / WAL / 隔离级别", 3, 5, 60, 1, 9 },
    { "画出查询执行全流程图", 3, 5, 30, 1, 10 },
    { "Phase 1 总结笔记", 3, 5, 30, 1, 11 },

    /* Phase 2: SQLite 源码精读（第 15-70 天） */
    { "Phase 2: SQLite 源码精读", 1, -1, 0, 3, 14 },
    { "W3: 全局骨架 + VDBE 概览", 2, 12, 0, 2, 14 },
    { "阅读 SQLite 架构文档", 3, 13, 45, 1, 14 },
    { "Trace SQL -> VDBE 字节码的全路径", 3, 13, 60, 1, 15 },
    { "理解 VDBE 虚拟机概览", 3, 13, 45, 1, 16 },
    { "W4: Tokenizer + Parser", 2, 12, 0, 2, 21 },
    { "阅读 tokenize.c 源码", 3, 17, 60, 1, 21 },
    { "理解 parse.y LALR 文法", 3, 17, 60, 1, 22 },
    { "Lemon 解析器生成流程", 3, 17, 45, 1, 23 },
    { "W5: Code Generator", 2, 12, 0, 2, 28 },
    { "阅读 build.c 源码", 3, 20, 60, 1, 28 },
    { "理解 SQL -> VDBE 指令序列", 3, 20, 60, 1, 29 },
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

    /* Phase 3: PostgreSQL 源码深入（第 71-300 天） */
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

int plan_import_template(int template_id, int64_t start_date, int64_t task_system_id) {
    if (template_id != TEMPLATE_DB_LEARNING) return -1;
    if (task_system_id <= 0) return -1;

    /* 1. 创建 plan */
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

    /* 2. 创建 plan_items，记录每个 item 的 ID 用于 parent_id 映射 */
    int64_t *id_map = (int64_t *)calloc(PLAN_ITEM_COUNT, sizeof(int64_t));
    if (!id_map) return -1;

    for (int i = 0; i < PLAN_ITEM_COUNT; i++) {
        plan_item_t item;
        memset(&item, 0, sizeof(item));
        item.plan_id = plan_id;

        /* 解析 parent_id：通过 id_map 查找父项的实际 ID */
        if (PLAN_ITEMS[i].parent_index >= 0 && PLAN_ITEMS[i].parent_index < i) {
            item.parent_id = id_map[PLAN_ITEMS[i].parent_index];
        } else {
            item.parent_id = 0;
        }

        strncpy(item.title, PLAN_ITEMS[i].title, ITEM_TITLE_MAX - 1);
        item.item_type = PLAN_ITEMS[i].item_type;
        item.estimated_minutes = PLAN_ITEMS[i].estimated_minutes;
        item.completion_rule = PLAN_ITEMS[i].completion_rule;
        item.planned_date = start_date + (int64_t)PLAN_ITEMS[i].days_offset * 86400;
        item.order_index = i;

        int64_t item_id;
        if (plan_item_create(&item, &item_id) == 0) {
            id_map[i] = item_id;
        }
    }

    free(id_map);
    return 0;
}

int plan_expand_to_todos(int64_t plan_id, int64_t task_system_id) {
    if (task_system_id <= 0) return -1;

    /* 1. 获取该 plan 的所有 item */
    plan_item_t *items = NULL;
    int count = 0;
    if (plan_item_list_by_plan(plan_id, &items, &count) != 0) return -1;
    if (count == 0) return -1;

    int created = 0;

    /* 2. 只展开 type=3（具体任务）的 item */
    for (int i = 0; i < count; i++) {
        if (items[i].item_type != 3) continue;
        if (items[i].planned_date == 0) continue;
        if (items[i].todo_id > 0) continue; /* 已展开过，跳过 */

        todo_t todo;
        memset(&todo, 0, sizeof(todo));
        strncpy(todo.title, items[i].title, TODO_TITLE_MAX - 1);
        snprintf(todo.description, TODO_DESC_MAX - 1, "来自计划项 ID=%lld", (long long)items[i].id);
        strcpy(todo.status, "open");
        todo.due_date = items[i].planned_date;
        todo.original_date = items[i].planned_date;
        todo.priority = 2; /* 中优先级 */
        todo.todo_type = 0; /* 普通 */
        todo.task_system_id = task_system_id;
        todo.plan_id = plan_id;
        todo.plan_item_id = items[i].id;

        int64_t todo_id;
        if (todo_create(&todo, &todo_id) == 0) {
            /* 回写 todo_id 到 plan_item */
            items[i].todo_id = todo_id;
            plan_item_update(&items[i]);
            created++;
        }
    }

    plan_item_list_free(items, count);
    return created;
}

int plan_get_phase_progress(int64_t plan_id, double *progress, int *count) {
    plan_item_t *items = NULL;
    int total = 0;
    if (plan_item_list_by_plan(plan_id, &items, &total) != 0) return -1;

    *count = 0;
    memset(progress, 0, sizeof(double) * 8);

    if (total == 0) { plan_item_list_free(items, total); return 0; }

    /* 找到所有 type=1（阶段）的项 */
    int phase_count = 0;
    for (int i = 0; i < total && phase_count < 8; i++) {
        if (items[i].item_type != 1) continue;
        int64_t phase_id = items[i].id;
        int total_tasks = 0;
        int completed_tasks = 0;

        for (int j = 0; j < total; j++) {
            if (items[j].parent_id == phase_id && items[j].item_type == 3) {
                total_tasks++;
                if (items[j].todo_id > 0) {
                    /* 检查该 todo 是否已完成 */
                    int idx = find_todo_idx(items[j].todo_id);
                    if (idx >= 0 && strcmp(g_todos[idx].status, "closed") == 0) {
                        completed_tasks++;
                    }
                }
            }
        }

        progress[phase_count] = total_tasks > 0 ? (double)completed_tasks / total_tasks * 100.0 : 0.0;
        phase_count++;
    }

    *count = phase_count;
    plan_item_list_free(items, total);
    return 0;
}