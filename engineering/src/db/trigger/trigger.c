/*
 * trigger.c - 触发器实现
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <db/trigger/trigger.h>

/* ─────────────────────────────────────────────────────────────────
 * 触发器结构
 * ───────────────────────────────────────────────────────────────── */

struct trigger {
    char *name;               /* 触发器名称 */
    char *table_name;        /* 关联表名 */
    trigger_event_t event;   /* 触发事件 */
    trigger_level_t level;   /* 触发粒度 */
    char *when_clause;       /* WHEN 条件 */
    char *action;           /* 触发动作 */
    bool enabled;           /* 是否启用 */
};

/* ─────────────────────────────────────────────────────────────────
 * 触发器管理器结构
 * ───────────────────────────────────────────────────────────────── */

struct trigger_manager {
    trigger_t **triggers;     /* 触发器数组 */
    int trigger_count;       /* 触发器数量 */
    int capacity;           /* 数组容量 */
};

/* ─────────────────────────────────────────────────────────────────
 * 触发器管理器实现
 * ───────────────────────────────────────────────────────────────── */

trigger_manager_t *trigger_manager_create(void)
{
    trigger_manager_t *mgr = (trigger_manager_t *)calloc(1, sizeof(trigger_manager_t));
    if (mgr == NULL) return NULL;

    mgr->capacity = 16;
    mgr->triggers = (trigger_t **)calloc(mgr->capacity, sizeof(trigger_t *));
    if (mgr->triggers == NULL) {
        free(mgr);
        return NULL;
    }

    mgr->trigger_count = 0;
    return mgr;
}

void trigger_manager_destroy(trigger_manager_t *mgr)
{
    if (mgr == NULL) return;

    for (int i = 0; i < mgr->trigger_count; i++) {
        if (mgr->triggers[i]) {
            free(mgr->triggers[i]->name);
            if (mgr->triggers[i]->table_name) free(mgr->triggers[i]->table_name);
            if (mgr->triggers[i]->when_clause) free(mgr->triggers[i]->when_clause);
            if (mgr->triggers[i]->action) free(mgr->triggers[i]->action);
            free(mgr->triggers[i]);
        }
    }
    free(mgr->triggers);
    free(mgr);
}

trigger_t *trigger_create(trigger_manager_t *mgr, const trigger_def_t *def)
{
    if (mgr == NULL || def == NULL) return NULL;

    /* 检查是否已存在 */
    for (int i = 0; i < mgr->trigger_count; i++) {
        if (mgr->triggers[i] && strcmp(mgr->triggers[i]->name, def->name) == 0) {
            return NULL;
        }
    }

    /* 扩展容量 */
    if (mgr->trigger_count >= mgr->capacity) {
        int new_cap = mgr->capacity * 2;
        trigger_t **new_triggers = (trigger_t **)realloc(
            mgr->triggers, new_cap * sizeof(trigger_t *));
        if (new_triggers == NULL) return NULL;
        mgr->triggers = new_triggers;
        mgr->capacity = new_cap;
    }

    trigger_t *trigger = (trigger_t *)calloc(1, sizeof(trigger_t));
    if (trigger == NULL) return NULL;

    trigger->name = strdup(def->name);
    trigger->table_name = def->table_name ? strdup(def->table_name) : NULL;
    trigger->event = def->event;
    trigger->level = def->level;
    trigger->when_clause = def->when_clause ? strdup(def->when_clause) : NULL;
    trigger->action = def->action ? strdup(def->action) : NULL;
    trigger->enabled = def->enabled;

    mgr->triggers[mgr->trigger_count++] = trigger;
    return trigger;
}

int trigger_drop(trigger_manager_t *mgr, const char *name)
{
    if (mgr == NULL || name == NULL) return -1;

    for (int i = 0; i < mgr->trigger_count; i++) {
        if (mgr->triggers[i] && strcmp(mgr->triggers[i]->name, name) == 0) {
            free(mgr->triggers[i]->name);
            if (mgr->triggers[i]->table_name) free(mgr->triggers[i]->table_name);
            if (mgr->triggers[i]->when_clause) free(mgr->triggers[i]->when_clause);
            if (mgr->triggers[i]->action) free(mgr->triggers[i]->action);
            free(mgr->triggers[i]);

            /* 移动数组 */
            for (int j = i; j < mgr->trigger_count - 1; j++) {
                mgr->triggers[j] = mgr->triggers[j + 1];
            }
            mgr->trigger_count--;
            return 0;
        }
    }
    return -1;
}

trigger_t *trigger_get(trigger_manager_t *mgr, const char *name)
{
    if (mgr == NULL || name == NULL) return NULL;

    for (int i = 0; i < mgr->trigger_count; i++) {
        if (mgr->triggers[i] && strcmp(mgr->triggers[i]->name, name) == 0) {
            return mgr->triggers[i];
        }
    }
    return NULL;
}

int trigger_enable(trigger_t *trigger)
{
    if (trigger == NULL) return -1;
    trigger->enabled = true;
    return 0;
}

int trigger_disable(trigger_t *trigger)
{
    if (trigger == NULL) return -1;
    trigger->enabled = false;
    return 0;
}

bool trigger_is_enabled(const trigger_t *trigger)
{
    return trigger ? trigger->enabled : false;
}

/* ─────────────────────────────────────────────────────────────────
 * 触发器执行实现
 * ───────────────────────────────────────────────────────────────── */

int trigger_fire_before(trigger_manager_t *mgr, const char *table_name,
                       trigger_event_t event, trigger_context_t *ctx)
{
    if (mgr == NULL || table_name == NULL) return 0;

    /* TODO: 检查 WHEN 条件 */

    for (int i = 0; i < mgr->trigger_count; i++) {
        trigger_t *t = mgr->triggers[i];
        if (t && t->enabled &&
            strcmp(t->table_name, table_name) == 0 &&
            t->event == event) {

            /* 执行触发器动作 */
            /* TODO: 实际执行 SQL 或函数 */
        }
    }

    (void)ctx;
    return 0;  /* 继续执行 */
}

void trigger_fire_after(trigger_manager_t *mgr, const char *table_name,
                       trigger_event_t event, trigger_context_t *ctx)
{
    if (mgr == NULL || table_name == NULL) return;

    for (int i = 0; i < mgr->trigger_count; i++) {
        trigger_t *t = mgr->triggers[i];
        if (t && t->enabled &&
            strcmp(t->table_name, table_name) == 0 &&
            t->event == event) {

            /* 执行触发器动作 */
            /* TODO: 实际执行 SQL 或函数 */
        }
    }

    (void)ctx;
}

/* ─────────────────────────────────────────────────────────────────
 * 触发器列表实现
 * ───────────────────────────────────────────────────────────────── */

char **trigger_list_for_table(const trigger_manager_t *mgr,
                             const char *table_name, int *count)
{
    if (mgr == NULL || table_name == NULL || count == NULL) return NULL;

    int cap = mgr->trigger_count < 8 ? mgr->trigger_count : 8;
    char **names = (char **)calloc(cap, sizeof(char *));
    if (names == NULL) return NULL;

    int n = 0;
    for (int i = 0; i < mgr->trigger_count; i++) {
        if (mgr->triggers[i] &&
            strcmp(mgr->triggers[i]->table_name, table_name) == 0) {
            names[n++] = strdup(mgr->triggers[i]->name);
        }
    }
    *count = n;

    return names;
}

char **trigger_list_all(const trigger_manager_t *mgr, int *count)
{
    if (mgr == NULL || count == NULL) return NULL;

    *count = mgr->trigger_count;
    if (mgr->trigger_count == 0) return NULL;

    char **names = (char **)malloc(mgr->trigger_count * sizeof(char *));
    for (int i = 0; i < mgr->trigger_count; i++) {
        names[i] = mgr->triggers[i] ? strdup(mgr->triggers[i]->name) : NULL;
    }
    return names;
}

/* ─────────────────────────────────────────────────────────────────
 * 辅助函数实现
 * ───────────────────────────────────────────────────────────────── */

trigger_def_t *trigger_def_create(const char *name, const char *table,
                                 trigger_event_t event, const char *action)
{
    trigger_def_t *def = (trigger_def_t *)calloc(1, sizeof(trigger_def_t));
    if (def == NULL) return NULL;

    def->name = name ? strdup(name) : NULL;
    def->table_name = table ? strdup(table) : NULL;
    def->event = event;
    def->level = TRIGGER_ROW;
    def->when_clause = NULL;
    def->action = action ? strdup(action) : NULL;
    def->enabled = true;

    return def;
}

void trigger_def_destroy(trigger_def_t *def)
{
    if (def == NULL) return;

    if (def->name) free(def->name);
    if (def->table_name) free(def->table_name);
    if (def->when_clause) free(def->when_clause);
    if (def->action) free(def->action);
    free(def);
}

const char *trigger_event_name(trigger_event_t event)
{
    switch (event) {
        case TRIGGER_BEFORE_INSERT: return "BEFORE INSERT";
        case TRIGGER_AFTER_INSERT:  return "AFTER INSERT";
        case TRIGGER_BEFORE_UPDATE: return "BEFORE UPDATE";
        case TRIGGER_AFTER_UPDATE:  return "AFTER UPDATE";
        case TRIGGER_BEFORE_DELETE: return "BEFORE DELETE";
        case TRIGGER_AFTER_DELETE:  return "AFTER DELETE";
        case TRIGGER_BEFORE_SELECT: return "BEFORE SELECT";
        case TRIGGER_AFTER_SELECT:  return "AFTER SELECT";
        default: return "UNKNOWN";
    }
}

const char *trigger_level_name(trigger_level_t level)
{
    switch (level) {
        case TRIGGER_ROW:       return "ROW";
        case TRIGGER_STATEMENT: return "STATEMENT";
        default: return "UNKNOWN";
    }
}

const char *trigger_fired_name(trigger_fired_t fired)
{
    switch (fired) {
        case TRIGGER_FIRED_BEFORE:    return "BEFORE";
        case TRIGGER_FIRED_AFTER:     return "AFTER";
        case TRIGGER_FIRED_INSTEAD_OF: return "INSTEAD OF";
        default: return "UNKNOWN";
    }
}