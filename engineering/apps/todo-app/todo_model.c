#include "todo_model.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ============================================================
 * 全局状态
 * ============================================================ */
static todo_t           *g_todos = NULL;
static int              g_todo_count = 0;
static int              g_todo_cap = 0;

static checklist_item_t *g_checks = NULL;
static int              g_check_count = 0;
static int              g_check_cap = 0;

static group_t          *g_groups = NULL;
static int              g_group_count = 0;
static int              g_group_cap = 0;

static comment_t        *g_comments = NULL;
static int              g_comment_count = 0;
static int              g_comment_cap = 0;

static task_system_t   *g_task_systems = NULL;
static int              g_ts_count = 0;
static int              g_ts_cap = 0;
static int64_t          g_next_ts_id = 1;

static plan_t          *g_plans = NULL;
static int              g_plan_count = 0;
static int              g_plan_cap = 0;
static int64_t          g_next_plan_id = 1;

static plan_item_t     *g_plan_items = NULL;
static int              g_pi_count = 0;
static int              g_pi_cap = 0;
static int64_t          g_next_pi_id = 1;

static int64_t          g_next_todo_id = 1;
static int64_t          g_next_check_id = 1;
static int64_t          g_next_group_id = 1;
static int64_t          g_next_comment_id = 1;

static char             g_db_path[512] = {0};
static int              g_modified = 0;

/* ============================================================
 * 容量扩展宏
 * ============================================================ */
#define ENSURE_CAP(arr, cap, count, type, init_cap) \
    do { \
        if (count >= cap) { \
            int new_cap = cap ? cap * 2 : init_cap; \
            while (new_cap <= count) new_cap *= 2; \
            arr = realloc(arr, new_cap * sizeof(type)); \
            memset(arr + cap, 0, (new_cap - cap) * sizeof(type)); \
            cap = new_cap; \
        } \
    } while(0)

#define ENSURE_TODO_CAP(n)  ENSURE_CAP(g_todos, g_todo_cap, n, todo_t, 16)
#define ENSURE_CHECK_CAP(n) ENSURE_CAP(g_checks, g_check_cap, n, checklist_item_t, 16)
#define ENSURE_GROUP_CAP(n) ENSURE_CAP(g_groups, g_group_cap, n, group_t, 8)
#define ENSURE_COMM_CAP(n)  ENSURE_CAP(g_comments, g_comment_cap, n, comment_t, 16)
#define ENSURE_TS_CAP(n)    ENSURE_CAP(g_task_systems, g_ts_cap, n, task_system_t, 8)
#define ENSURE_PLAN_CAP(n)  ENSURE_CAP(g_plans, g_plan_cap, n, plan_t, 8)
#define ENSURE_PI_CAP(n)    ENSURE_CAP(g_plan_items, g_pi_cap, n, plan_item_t, 16)

/* ============================================================
 * 工具函数
 * ============================================================ */
static int64_t now_ts(void) {
    return (int64_t)time(NULL);
}

static int find_todo_idx(int64_t id) {
    for (int i = 0; i < g_todo_count; i++) {
        if (g_todos[i].id == id) return i;
    }
    return -1;
}

static int find_check_idx(int64_t id) {
    for (int i = 0; i < g_check_count; i++) {
        if (g_checks[i].id == id) return i;
    }
    return -1;
}

static int find_group_idx(int64_t id) {
    for (int i = 0; i < g_group_count; i++) {
        if (g_groups[i].id == id) return i;
    }
    return -1;
}

static int find_comment_idx(int64_t id) {
    for (int i = 0; i < g_comment_count; i++) {
        if (g_comments[i].id == id) return i;
    }
    return -1;
}

static int find_ts_idx(int64_t id) {
    for (int i = 0; i < g_ts_count; i++) {
        if (g_task_systems[i].id == id) return i;
    }
    return -1;
}

static int find_plan_idx(int64_t id) {
    for (int i = 0; i < g_plan_count; i++) {
        if (g_plans[i].id == id) return i;
    }
    return -1;
}

static int find_pi_idx(int64_t id) {
    for (int i = 0; i < g_pi_count; i++) {
        if (g_plan_items[i].id == id) return i;
    }
    return -1;
}

/* ============================================================
 * 持久化（JSON 文件）
 * ============================================================ */
static cJSON *todos_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < g_todo_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", g_todos[i].id);
        cJSON_AddStringToObject(o, "title", g_todos[i].title);
        cJSON_AddStringToObject(o, "description", g_todos[i].description);
        cJSON_AddStringToObject(o, "status", g_todos[i].status);
        cJSON_AddNumberToObject(o, "priority", g_todos[i].priority);
        cJSON_AddNumberToObject(o, "due_date", g_todos[i].due_date);
        cJSON_AddNumberToObject(o, "group_id", g_todos[i].group_id);
        cJSON_AddNumberToObject(o, "sort_order", g_todos[i].sort_order);

        cJSON *labels = cJSON_Parse(g_todos[i].labels);
        if (labels && cJSON_IsArray(labels)) {
            cJSON_AddItemToObject(o, "labels", labels);
        } else {
            cJSON_AddItemToObject(o, "labels", cJSON_CreateArray());
            if (labels) cJSON_Delete(labels);
        }

        cJSON_AddNumberToObject(o, "created_at", g_todos[i].created_at);
        cJSON_AddNumberToObject(o, "updated_at", g_todos[i].updated_at);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static cJSON *checks_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < g_check_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", g_checks[i].id);
        cJSON_AddNumberToObject(o, "todo_id", g_checks[i].todo_id);
        cJSON_AddStringToObject(o, "text", g_checks[i].text);
        cJSON_AddBoolToObject(o, "done", g_checks[i].done ? 1 : 0);
        cJSON_AddNumberToObject(o, "sort_order", g_checks[i].sort_order);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static cJSON *groups_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < g_group_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", g_groups[i].id);
        cJSON_AddStringToObject(o, "name", g_groups[i].name);
        cJSON_AddStringToObject(o, "color", g_groups[i].color);
        cJSON_AddNumberToObject(o, "sort_order", g_groups[i].sort_order);
        cJSON_AddNumberToObject(o, "created_at", g_groups[i].created_at);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static cJSON *comments_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < g_comment_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", g_comments[i].id);
        cJSON_AddNumberToObject(o, "todo_id", g_comments[i].todo_id);
        cJSON_AddStringToObject(o, "text", g_comments[i].text);
        cJSON_AddNumberToObject(o, "created_at", g_comments[i].created_at);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static cJSON *task_systems_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < g_ts_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", g_task_systems[i].id);
        cJSON_AddStringToObject(o, "name", g_task_systems[i].name);
        cJSON_AddStringToObject(o, "description", g_task_systems[i].description);
        cJSON_AddStringToObject(o, "color", g_task_systems[i].color);
        cJSON_AddNumberToObject(o, "sort_order", g_task_systems[i].sort_order);
        cJSON_AddNumberToObject(o, "created_at", g_task_systems[i].created_at);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static cJSON *plans_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < g_plan_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", g_plans[i].id);
        cJSON_AddStringToObject(o, "name", g_plans[i].name);
        cJSON_AddStringToObject(o, "description", g_plans[i].description);
        cJSON_AddNumberToObject(o, "start_date", g_plans[i].start_date);
        cJSON_AddNumberToObject(o, "end_date", g_plans[i].end_date);
        cJSON_AddStringToObject(o, "color", g_plans[i].color);
        cJSON_AddNumberToObject(o, "status", g_plans[i].status);
        cJSON_AddNumberToObject(o, "created_at", g_plans[i].created_at);
        cJSON_AddNumberToObject(o, "updated_at", g_plans[i].updated_at);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static cJSON *plan_items_to_json(void) {
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < g_pi_count; i++) {
        cJSON *o = cJSON_CreateObject();
        cJSON_AddNumberToObject(o, "id", g_plan_items[i].id);
        cJSON_AddNumberToObject(o, "plan_id", g_plan_items[i].plan_id);
        cJSON_AddNumberToObject(o, "parent_id", g_plan_items[i].parent_id);
        cJSON_AddStringToObject(o, "title", g_plan_items[i].title);
        cJSON_AddNumberToObject(o, "item_type", g_plan_items[i].item_type);
        cJSON_AddNumberToObject(o, "planned_date", g_plan_items[i].planned_date);
        cJSON_AddNumberToObject(o, "estimated_minutes", g_plan_items[i].estimated_minutes);
        cJSON_AddNumberToObject(o, "order_index", g_plan_items[i].order_index);
        cJSON_AddNumberToObject(o, "completion_rule", g_plan_items[i].completion_rule);
        cJSON_AddNumberToObject(o, "todo_id", g_plan_items[i].todo_id);
        cJSON_AddNumberToObject(o, "actual_minutes", g_plan_items[i].actual_minutes);
        cJSON_AddItemToArray(arr, o);
    }
    return arr;
}

static int persist_now(void) {
    if (!g_db_path[0]) return 0;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "next_todo_id", g_next_todo_id);
    cJSON_AddNumberToObject(root, "next_check_id", g_next_check_id);
    cJSON_AddNumberToObject(root, "next_group_id", g_next_group_id);
    cJSON_AddNumberToObject(root, "next_comment_id", g_next_comment_id);
    cJSON_AddNumberToObject(root, "next_ts_id", g_next_ts_id);
    cJSON_AddNumberToObject(root, "next_plan_id", g_next_plan_id);
    cJSON_AddNumberToObject(root, "next_pi_id", g_next_pi_id);
    cJSON_AddItemToObject(root, "todos", todos_to_json());
    cJSON_AddItemToObject(root, "checklist", checks_to_json());
    cJSON_AddItemToObject(root, "groups", groups_to_json());
    cJSON_AddItemToObject(root, "comments", comments_to_json());
    cJSON_AddItemToObject(root, "task_systems", task_systems_to_json());
    cJSON_AddItemToObject(root, "plans", plans_to_json());
    cJSON_AddItemToObject(root, "plan_items", plan_items_to_json());

    char *out = cJSON_Print(root);
    cJSON_Delete(root);

    if (!out) return -1;

    /* 临时文件 + rename 防中途崩溃 */
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_db_path);

    FILE *fp = fopen(tmp, "w");
    if (!fp) { free(out); return -1; }
    fputs(out, fp);
    fclose(fp);
    free(out);

    /* 确保数据刷到磁盘 */
    {
        FILE *dst = fopen(g_db_path, "ab");
        if (dst) fclose(dst);
    }

    /* Windows 下 rename 不会覆盖已存在文件，先删除 */
    remove(g_db_path);
    if (rename(tmp, g_db_path) != 0) {
        return -1;
    }
    g_modified = 0;
    return 0;
}

/* ============================================================
 * JSON 解析
 * ============================================================ */
static void parse_todo(cJSON *o) {
    ENSURE_TODO_CAP(g_todo_count + 1);
    todo_t *it = &g_todos[g_todo_count];
    memset(it, 0, sizeof(*it));

    cJSON *jid = cJSON_GetObjectItem(o, "id");
    it->id = jid ? (int64_t)jid->valuedouble : 0;

    cJSON *jtitle = cJSON_GetObjectItem(o, "title");
    if (jtitle && cJSON_IsString(jtitle)) {
        strncpy(it->title, jtitle->valuestring, TODO_TITLE_MAX - 1);
    }

    cJSON *jdesc = cJSON_GetObjectItem(o, "description");
    if (jdesc && cJSON_IsString(jdesc)) {
        strncpy(it->description, jdesc->valuestring, TODO_DESC_MAX - 1);
    }

    cJSON *jstatus = cJSON_GetObjectItem(o, "status");
    if (jstatus && cJSON_IsString(jstatus)) {
        strncpy(it->status, jstatus->valuestring, TODO_STATUS_MAX - 1);
    } else {
        strncpy(it->status, "open", TODO_STATUS_MAX - 1);
    }

    cJSON *jpriority = cJSON_GetObjectItem(o, "priority");
    it->priority = jpriority ? (int)jpriority->valuedouble : PRIORITY_NONE;

    cJSON *jdue = cJSON_GetObjectItem(o, "due_date");
    it->due_date = jdue ? (int64_t)jdue->valuedouble : 0;

    cJSON *jgid = cJSON_GetObjectItem(o, "group_id");
    it->group_id = jgid ? (int64_t)jgid->valuedouble : 0;

    cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
    it->sort_order = jso ? (int)jso->valuedouble : 0;

    cJSON *jlabels = cJSON_GetObjectItem(o, "labels");
    if (jlabels && cJSON_IsArray(jlabels)) {
        char *ls = cJSON_PrintUnformatted(jlabels);
        if (ls) {
            strncpy(it->labels, ls, TODO_LABELS_MAX - 1);
            free(ls);
        }
    } else if (jlabels && cJSON_IsString(jlabels)) {
        strncpy(it->labels, jlabels->valuestring, TODO_LABELS_MAX - 1);
    } else {
        strcpy(it->labels, "[]");
    }

    cJSON *jca = cJSON_GetObjectItem(o, "created_at");
    it->created_at = jca ? (int64_t)jca->valuedouble : 0;
    cJSON *jua = cJSON_GetObjectItem(o, "updated_at");
    it->updated_at = jua ? (int64_t)jua->valuedouble : 0;

    /* 解析新增字段 */
    cJSON *jtodo_type = cJSON_GetObjectItem(o, "todo_type");
    it->todo_type = jtodo_type ? (int)jtodo_type->valuedouble : 0;

    cJSON *jorig_date = cJSON_GetObjectItem(o, "original_date");
    it->original_date = jorig_date ? (int64_t)jorig_date->valuedouble : 0;

    cJSON *jcarryover = cJSON_GetObjectItem(o, "carryover_count");
    it->carryover_count = jcarryover ? (int)jcarryover->valuedouble : 0;

    cJSON *jplan_id = cJSON_GetObjectItem(o, "plan_id");
    it->plan_id = jplan_id ? (int64_t)jplan_id->valuedouble : 0;

    cJSON *jplan_item_id = cJSON_GetObjectItem(o, "plan_item_id");
    it->plan_item_id = jplan_item_id ? (int64_t)jplan_item_id->valuedouble : 0;

    cJSON *jcompleted_at = cJSON_GetObjectItem(o, "completed_at");
    it->completed_at = jcompleted_at ? (int64_t)jcompleted_at->valuedouble : 0;

    cJSON *jpostpone = cJSON_GetObjectItem(o, "postpone_until");
    it->postpone_until = jpostpone ? (int64_t)jpostpone->valuedouble : 0;

    cJSON *jtask_sys_id = cJSON_GetObjectItem(o, "task_system_id");
    it->task_system_id = jtask_sys_id ? (int64_t)jtask_sys_id->valuedouble : 0;

    g_todo_count++;
}

static void parse_check(cJSON *o) {
    ENSURE_CHECK_CAP(g_check_count + 1);
    checklist_item_t *c = &g_checks[g_check_count];
    memset(c, 0, sizeof(*c));

    cJSON *jid = cJSON_GetObjectItem(o, "id");
    c->id = jid ? (int64_t)jid->valuedouble : 0;

    /* 兼容旧字段：issue_id 和 todo_id */
    cJSON *jtid = cJSON_GetObjectItem(o, "todo_id");
    if (!jtid) jtid = cJSON_GetObjectItem(o, "issue_id"); /* 兼容旧数据 */
    c->todo_id = jtid ? (int64_t)jtid->valuedouble : 0;

    cJSON *jtext = cJSON_GetObjectItem(o, "text");
    if (jtext && cJSON_IsString(jtext)) {
        strncpy(c->text, jtext->valuestring, CHECKLIST_TEXT_MAX - 1);
    }

    cJSON *jdone = cJSON_GetObjectItem(o, "done");
    if (jdone) c->done = (cJSON_IsBool(jdone) ? jdone->valueint : (int)jdone->valuedouble);

    cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
    c->sort_order = jso ? (int)jso->valuedouble : 0;

    g_check_count++;
}

static void parse_group(cJSON *o) {
    ENSURE_GROUP_CAP(g_group_count + 1);
    group_t *g = &g_groups[g_group_count];
    memset(g, 0, sizeof(*g));

    cJSON *jid = cJSON_GetObjectItem(o, "id");
    g->id = jid ? (int64_t)jid->valuedouble : 0;

    cJSON *jname = cJSON_GetObjectItem(o, "name");
    if (jname && cJSON_IsString(jname)) {
        strncpy(g->name, jname->valuestring, GROUP_NAME_MAX - 1);
    }

    cJSON *jcolor = cJSON_GetObjectItem(o, "color");
    if (jcolor && cJSON_IsString(jcolor)) {
        strncpy(g->color, jcolor->valuestring, GROUP_COLOR_MAX - 1);
    } else {
        strcpy(g->color, "#4A90D9");
    }

    cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
    g->sort_order = jso ? (int)jso->valuedouble : 0;

    cJSON *jca = cJSON_GetObjectItem(o, "created_at");
    g->created_at = jca ? (int64_t)jca->valuedouble : 0;

    g_group_count++;
}

static void parse_comment(cJSON *o) {
    ENSURE_COMM_CAP(g_comment_count + 1);
    comment_t *c = &g_comments[g_comment_count];
    memset(c, 0, sizeof(*c));

    cJSON *jid = cJSON_GetObjectItem(o, "id");
    c->id = jid ? (int64_t)jid->valuedouble : 0;

    cJSON *jtid = cJSON_GetObjectItem(o, "todo_id");
    c->todo_id = jtid ? (int64_t)jtid->valuedouble : 0;

    cJSON *jtext = cJSON_GetObjectItem(o, "text");
    if (jtext && cJSON_IsString(jtext)) {
        strncpy(c->text, jtext->valuestring, COMMENT_TEXT_MAX - 1);
    }

    cJSON *jca = cJSON_GetObjectItem(o, "created_at");
    c->created_at = jca ? (int64_t)jca->valuedouble : 0;

    g_comment_count++;
}

static void parse_task_system(cJSON *o) {
    ENSURE_TS_CAP(g_ts_count + 1);
    task_system_t *ts = &g_task_systems[g_ts_count];
    memset(ts, 0, sizeof(*ts));

    cJSON *jid = cJSON_GetObjectItem(o, "id");
    ts->id = jid ? (int64_t)jid->valuedouble : 0;

    cJSON *jname = cJSON_GetObjectItem(o, "name");
    if (jname && cJSON_IsString(jname)) {
        strncpy(ts->name, jname->valuestring, TASK_SYSTEM_NAME_MAX - 1);
    }

    cJSON *jdesc = cJSON_GetObjectItem(o, "description");
    if (jdesc && cJSON_IsString(jdesc)) {
        strncpy(ts->description, jdesc->valuestring, 1023);
    }

    cJSON *jcolor = cJSON_GetObjectItem(o, "color");
    if (jcolor && cJSON_IsString(jcolor)) {
        strncpy(ts->color, jcolor->valuestring, 15);
    }

    cJSON *jso = cJSON_GetObjectItem(o, "sort_order");
    ts->sort_order = jso ? (int)jso->valuedouble : 0;

    cJSON *jca = cJSON_GetObjectItem(o, "created_at");
    ts->created_at = jca ? (int64_t)jca->valuedouble : 0;

    g_ts_count++;
}

static void parse_plan(cJSON *o) {
    ENSURE_PLAN_CAP(g_plan_count + 1);
    plan_t *p = &g_plans[g_plan_count];
    memset(p, 0, sizeof(*p));

    cJSON *jid = cJSON_GetObjectItem(o, "id");
    p->id = jid ? (int64_t)jid->valuedouble : 0;

    cJSON *jname = cJSON_GetObjectItem(o, "name");
    if (jname && cJSON_IsString(jname)) {
        strncpy(p->name, jname->valuestring, PLAN_NAME_MAX - 1);
    }

    cJSON *jdesc = cJSON_GetObjectItem(o, "description");
    if (jdesc && cJSON_IsString(jdesc)) {
        strncpy(p->description, jdesc->valuestring, PLAN_DESC_MAX - 1);
    }

    cJSON *jstart = cJSON_GetObjectItem(o, "start_date");
    p->start_date = jstart ? (int64_t)jstart->valuedouble : 0;

    cJSON *jend = cJSON_GetObjectItem(o, "end_date");
    p->end_date = jend ? (int64_t)jend->valuedouble : 0;

    cJSON *jcolor = cJSON_GetObjectItem(o, "color");
    if (jcolor && cJSON_IsString(jcolor)) {
        strncpy(p->color, jcolor->valuestring, 15);
    }

    cJSON *jstatus = cJSON_GetObjectItem(o, "status");
    p->status = jstatus ? (int)jstatus->valuedouble : 0;

    cJSON *jca = cJSON_GetObjectItem(o, "created_at");
    p->created_at = jca ? (int64_t)jca->valuedouble : 0;

    cJSON *jua = cJSON_GetObjectItem(o, "updated_at");
    p->updated_at = jua ? (int64_t)jua->valuedouble : 0;

    g_plan_count++;
}

static void parse_plan_item(cJSON *o) {
    ENSURE_PI_CAP(g_pi_count + 1);
    plan_item_t *pi = &g_plan_items[g_pi_count];
    memset(pi, 0, sizeof(*pi));

    cJSON *jid = cJSON_GetObjectItem(o, "id");
    pi->id = jid ? (int64_t)jid->valuedouble : 0;

    cJSON *jplan_id = cJSON_GetObjectItem(o, "plan_id");
    pi->plan_id = jplan_id ? (int64_t)jplan_id->valuedouble : 0;

    cJSON *jparent = cJSON_GetObjectItem(o, "parent_id");
    pi->parent_id = jparent ? (int64_t)jparent->valuedouble : 0;

    cJSON *jtitle = cJSON_GetObjectItem(o, "title");
    if (jtitle && cJSON_IsString(jtitle)) {
        strncpy(pi->title, jtitle->valuestring, ITEM_TITLE_MAX - 1);
    }

    cJSON *jtype = cJSON_GetObjectItem(o, "item_type");
    pi->item_type = jtype ? (int)jtype->valuedouble : 0;

    cJSON *jpdate = cJSON_GetObjectItem(o, "planned_date");
    pi->planned_date = jpdate ? (int64_t)jpdate->valuedouble : 0;

    cJSON *jest = cJSON_GetObjectItem(o, "estimated_minutes");
    pi->estimated_minutes = jest ? (int)jest->valuedouble : 0;

    cJSON *jorder = cJSON_GetObjectItem(o, "order_index");
    pi->order_index = jorder ? (int)jorder->valuedouble : 0;

    cJSON *jrule = cJSON_GetObjectItem(o, "completion_rule");
    pi->completion_rule = jrule ? (int)jrule->valuedouble : 0;

    cJSON *jtodo_id = cJSON_GetObjectItem(o, "todo_id");
    pi->todo_id = jtodo_id ? (int64_t)jtodo_id->valuedouble : 0;

    cJSON *jactual = cJSON_GetObjectItem(o, "actual_minutes");
    pi->actual_minutes = jactual ? (int)jactual->valuedouble : 0;

    g_pi_count++;
}

/* ============================================================
 * 数据库生命周期
 * ============================================================ */
int todo_db_load(const char *db_path) {
    if (!db_path) return -1;
    strncpy(g_db_path, db_path, sizeof(g_db_path) - 1);
    g_db_path[sizeof(g_db_path) - 1] = '\0';

    /* 文件不存在即空库 */
    FILE *fp = fopen(g_db_path, "r");
    if (!fp) {
        return 0;
    }
    fseek(fp, 0, SEEK_END);
    long sz = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (sz <= 0) { fclose(fp); return 0; }

    char *buf = malloc(sz + 1);
    if (!buf) { fclose(fp); return -1; }
    fread(buf, 1, sz, fp);
    buf[sz] = '\0';
    fclose(fp);

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root) return -1;

    /* 读取 next_id */
    cJSON *jnext = cJSON_GetObjectItem(root, "next_todo_id");
    g_next_todo_id = jnext ? (int64_t)jnext->valuedouble : 1;

    jnext = cJSON_GetObjectItem(root, "next_check_id");
    g_next_check_id = jnext ? (int64_t)jnext->valuedouble : 1;

    jnext = cJSON_GetObjectItem(root, "next_group_id");
    g_next_group_id = jnext ? (int64_t)jnext->valuedouble : 1;

    jnext = cJSON_GetObjectItem(root, "next_comment_id");
    g_next_comment_id = jnext ? (int64_t)jnext->valuedouble : 1;

    jnext = cJSON_GetObjectItem(root, "next_ts_id");
    g_next_ts_id = jnext ? (int64_t)jnext->valuedouble : 1;

    jnext = cJSON_GetObjectItem(root, "next_plan_id");
    g_next_plan_id = jnext ? (int64_t)jnext->valuedouble : 1;

    jnext = cJSON_GetObjectItem(root, "next_pi_id");
    g_next_pi_id = jnext ? (int64_t)jnext->valuedouble : 1;

    /* 读取 todos */
    cJSON *jtodos = cJSON_GetObjectItem(root, "todos");
    if (jtodos && cJSON_IsArray(jtodos)) {
        int n = cJSON_GetArraySize(jtodos);
        for (int i = 0; i < n; i++) {
            parse_todo(cJSON_GetArrayItem(jtodos, i));
        }
    }

    /* 读取 checklist */
    cJSON *jchecks = cJSON_GetObjectItem(root, "checklist");
    if (jchecks && cJSON_IsArray(jchecks)) {
        int n = cJSON_GetArraySize(jchecks);
        for (int i = 0; i < n; i++) {
            parse_check(cJSON_GetArrayItem(jchecks, i));
        }
    }

    /* 读取 groups */
    cJSON *jgroups = cJSON_GetObjectItem(root, "groups");
    if (jgroups && cJSON_IsArray(jgroups)) {
        int n = cJSON_GetArraySize(jgroups);
        for (int i = 0; i < n; i++) {
            parse_group(cJSON_GetArrayItem(jgroups, i));
        }
    }

    /* 读取 comments */
    cJSON *jcomments = cJSON_GetObjectItem(root, "comments");
    if (jcomments && cJSON_IsArray(jcomments)) {
        int n = cJSON_GetArraySize(jcomments);
        for (int i = 0; i < n; i++) {
            parse_comment(cJSON_GetArrayItem(jcomments, i));
        }
    }

    /* 读取 task_systems */
    cJSON *jts = cJSON_GetObjectItem(root, "task_systems");
    if (jts && cJSON_IsArray(jts)) {
        int n = cJSON_GetArraySize(jts);
        for (int i = 0; i < n; i++) {
            parse_task_system(cJSON_GetArrayItem(jts, i));
        }
    }

    /* 读取 plans */
    cJSON *jplans = cJSON_GetObjectItem(root, "plans");
    if (jplans && cJSON_IsArray(jplans)) {
        int n = cJSON_GetArraySize(jplans);
        for (int i = 0; i < n; i++) {
            parse_plan(cJSON_GetArrayItem(jplans, i));
        }
    }

    /* 读取 plan_items */
    cJSON *jpitems = cJSON_GetObjectItem(root, "plan_items");
    if (jpitems && cJSON_IsArray(jpitems)) {
        int n = cJSON_GetArraySize(jpitems);
        for (int i = 0; i < n; i++) {
            parse_plan_item(cJSON_GetArrayItem(jpitems, i));
        }
    }

    /* 确保存在默认任务系统 */
    if (g_ts_count == 0) {
        task_system_t def_ts;
        memset(&def_ts, 0, sizeof(def_ts));
        strncpy(def_ts.name, "默认任务系统", TASK_SYSTEM_NAME_MAX - 1);
        strncpy(def_ts.color, "#4A90D9", 15);
        task_system_create(&def_ts, NULL);
    }

    cJSON_Delete(root);
    return 0;
}

int todo_db_save(void) {
    g_modified = 1;
    return persist_now();
}

void todo_db_shutdown(void) {
    if (g_modified) persist_now();
    free(g_todos);    g_todos = NULL;
    free(g_checks);   g_checks = NULL;
    free(g_groups);   g_groups = NULL;
    free(g_comments); g_comments = NULL;
    free(g_task_systems); g_task_systems = NULL;
    free(g_plans);       g_plans = NULL;
    free(g_plan_items);  g_plan_items = NULL;
    g_todo_count = g_todo_cap = 0;
    g_check_count = g_check_cap = 0;
    g_group_count = g_group_cap = 0;
    g_comment_count = g_comment_cap = 0;
    g_ts_count = g_ts_cap = 0;
    g_plan_count = g_plan_cap = 0;
    g_pi_count = g_pi_cap = 0;
}

void todo_db_reset(void) {
    /* 仅清除内存数据，不释放指针，用于测试 */
    free(g_todos);    g_todos = NULL;
    free(g_checks);   g_checks = NULL;
    free(g_groups);   g_groups = NULL;
    free(g_comments); g_comments = NULL;
    free(g_task_systems); g_task_systems = NULL;
    free(g_plans);       g_plans = NULL;
    free(g_plan_items);  g_plan_items = NULL;
    g_todo_count = g_todo_cap = 0;
    g_check_count = g_check_cap = 0;
    g_group_count = g_group_cap = 0;
    g_comment_count = g_comment_cap = 0;
    g_ts_count = g_ts_cap = 0;
    g_plan_count = g_plan_cap = 0;
    g_pi_count = g_pi_cap = 0;
    g_next_todo_id = 1;
    g_next_check_id = 1;
    g_next_group_id = 1;
    g_next_comment_id = 1;
    g_next_ts_id = 1;
    g_next_plan_id = 1;
    g_next_pi_id = 1;
    g_db_path[0] = '\0';
    g_modified = 0;
}

/* ============================================================
 * 标签匹配
 * ============================================================ */
static int match_labels(const char *todo_labels, const char *query_labels) {
    if (!query_labels || !query_labels[0]) return 1;
    if (!todo_labels || !todo_labels[0]) return 0;

    char qcpy[TODO_LABELS_MAX];
    strncpy(qcpy, query_labels, sizeof(qcpy) - 1);
    qcpy[sizeof(qcpy) - 1] = '\0';

    char *save = NULL;
    char *tok = strtok_r(qcpy, ",", &save);
    while (tok) {
        while (*tok == ' ') tok++;
        size_t tlen = strlen(tok);
        while (tlen > 0 && (tok[tlen-1] == ' ' || tok[tlen-1] == '"')) tlen--;
        tok[tlen] = '\0';

        if (tok[0] && strstr(todo_labels, tok) == NULL) return 0;
        tok = strtok_r(NULL, ",", &save);
    }
    return 1;
}

/* ============================================================
 * Todo CRUD
 * ============================================================ */
int todo_create(const todo_t *todo, int64_t *out_id) {
    ENSURE_TODO_CAP(g_todo_count + 1);
    todo_t *it = &g_todos[g_todo_count];
    memset(it, 0, sizeof(*it));
    it->id = g_next_todo_id++;

    strncpy(it->title, todo->title, TODO_TITLE_MAX - 1);
    strncpy(it->description, todo->description, TODO_DESC_MAX - 1);
    if (todo->status[0])
        strncpy(it->status, todo->status, TODO_STATUS_MAX - 1);
    else
        strncpy(it->status, "open", TODO_STATUS_MAX - 1);

    if (todo->labels[0])
        strncpy(it->labels, todo->labels, TODO_LABELS_MAX - 1);
    else
        strcpy(it->labels, "[]");

    it->priority = todo->priority;
    it->due_date = todo->due_date;
    it->group_id = todo->group_id;
    it->sort_order = todo->sort_order;
    it->todo_type = todo->todo_type;
    it->original_date = todo->original_date;
    it->carryover_count = todo->carryover_count;
    it->plan_id = todo->plan_id;
    it->plan_item_id = todo->plan_item_id;
    it->completed_at = todo->completed_at;
    it->postpone_until = todo->postpone_until;
    it->task_system_id = todo->task_system_id;
    it->created_at = now_ts();
    it->updated_at = it->created_at;

    g_todo_count++;
    if (out_id) *out_id = it->id;
    todo_db_save();
    return 0;
}

int todo_get_by_id(int64_t id, todo_t *todo) {
    int idx = find_todo_idx(id);
    if (idx < 0) return -1;
    if (todo) *todo = g_todos[idx];
    return 0;
}

int todo_update(const todo_t *todo) {
    int idx = find_todo_idx(todo->id);
    if (idx < 0) return -1;
    todo_t *it = &g_todos[idx];

    strncpy(it->title, todo->title, TODO_TITLE_MAX - 1);
    strncpy(it->description, todo->description, TODO_DESC_MAX - 1);
    strncpy(it->status, todo->status, TODO_STATUS_MAX - 1);
    strncpy(it->labels, todo->labels, TODO_LABELS_MAX - 1);
    it->priority = todo->priority;
    it->due_date = todo->due_date;
    it->group_id = todo->group_id;
    it->sort_order = todo->sort_order;
    it->todo_type = todo->todo_type;
    it->original_date = todo->original_date;
    it->carryover_count = todo->carryover_count;
    it->plan_id = todo->plan_id;
    it->plan_item_id = todo->plan_item_id;
    it->completed_at = todo->completed_at;
    it->postpone_until = todo->postpone_until;
    it->task_system_id = todo->task_system_id;
    it->updated_at = now_ts();
    todo_db_save();
    return 0;
}

int todo_delete(int64_t id) {
    int idx = find_todo_idx(id);
    if (idx < 0) return -1;

    /* 级联删除 checklist */
    int j = 0;
    for (int i = 0; i < g_check_count; i++) {
        if (g_checks[i].todo_id != id) {
            g_checks[j++] = g_checks[i];
        }
    }
    g_check_count = j;

    /* 级联删除评论 */
    j = 0;
    for (int i = 0; i < g_comment_count; i++) {
        if (g_comments[i].todo_id != id) {
            g_comments[j++] = g_comments[i];
        }
    }
    g_comment_count = j;

    /* 删除 todo */
    for (int i = idx; i < g_todo_count - 1; i++) {
        g_todos[i] = g_todos[i+1];
    }
    g_todo_count--;
    todo_db_save();
    return 0;
}

int todo_list(const todo_query_t *query, todo_list_t *result) {
    memset(result, 0, sizeof(*result));

    /* 第一遍：筛选 */
    todo_t *matches = NULL;
    int mcount = 0, mcap = 0;

    int64_t now = now_ts();

    for (int i = 0; i < g_todo_count; i++) {
        todo_t *it = &g_todos[i];

        if (query->status && strcmp(query->status, "all") != 0) {
            if (strcmp(it->status, query->status) != 0) continue;
        }
        if (query->labels && query->labels[0]) {
            if (!match_labels(it->labels, query->labels)) continue;
        }
        if (query->search && query->search[0]) {
            if (strstr(it->title, query->search) == NULL &&
                strstr(it->description, query->search) == NULL) continue;
        }
        if (query->priority >= 0 && it->priority != query->priority) continue;
        if (query->group_id > 0 && it->group_id != query->group_id) continue;
        if (query->group_id == 0 && it->group_id != 0) continue;  /* 只查未分组 */
        if (query->due_before > 0 && it->due_date > 0 && it->due_date > query->due_before) continue;
        if (query->due_after > 0 && it->due_date > 0 && it->due_date < query->due_after) continue;

        if (mcount >= mcap) {
            mcap = mcap ? mcap * 2 : 16;
            matches = realloc(matches, mcap * sizeof(todo_t));
        }
        matches[mcount++] = *it;
    }

    /* 排序 */
    if (matches && mcount > 1) {
        const char *sort_field = query->sort ? query->sort : "sort_order";
        int desc = query->sort_desc ? -1 : 1;

        for (int i = 0; i < mcount - 1; i++) {
            for (int k = i + 1; k < mcount; k++) {
                int swap = 0;
                if (strcmp(sort_field, "priority") == 0) {
                    swap = (matches[i].priority * desc) > (matches[k].priority * desc);
                } else if (strcmp(sort_field, "due_date") == 0) {
                    swap = matches[i].due_date > matches[k].due_date;
                    if (desc < 0) swap = !swap;
                } else if (strcmp(sort_field, "created_at") == 0) {
                    swap = matches[i].created_at < matches[k].created_at;
                    if (desc < 0) swap = !swap;
                } else {
                    /* 默认 sort_order */
                    swap = matches[i].sort_order > matches[k].sort_order;
                    if (desc < 0) swap = !swap;
                }
                if (swap) {
                    todo_t tmp = matches[i];
                    matches[i] = matches[k];
                    matches[k] = tmp;
                }
            }
        }
    }

    result->total = mcount;

    int p = query->page > 0 ? query->page : 1;
    int pp = query->per_page > 0 ? query->per_page : 20;
    if (pp > 100) pp = 100;

    int start = (p - 1) * pp;
    if (start < mcount) {
        int end = start + pp;
        if (end > mcount) end = mcount;
        result->count = end - start;
        result->items = malloc(result->count * sizeof(todo_t));
        memcpy(result->items, matches + start, result->count * sizeof(todo_t));
    } else {
        result->count = 0;
        result->items = NULL;
    }

    free(matches);
    return 0;
}

void todo_list_free(todo_list_t *result) {
    if (!result) return;
    free(result->items);
    result->items = NULL;
    result->count = 0;
    result->total = 0;
}

int todo_update_sort(int64_t id, int sort_order) {
    int idx = find_todo_idx(id);
    if (idx < 0) return -1;
    g_todos[idx].sort_order = sort_order;
    g_todos[idx].updated_at = now_ts();
    todo_db_save();
    return 0;
}

int todo_get_stats(todo_stats_t *stats) {
    memset(stats, 0, sizeof(*stats));
    int64_t now = now_ts();
    int64_t today_end = (now / 86400 + 1) * 86400;  /* 今天 23:59:59 */
    int64_t today_start = (now / 86400) * 86400;      /* 今天 00:00:00 */

    for (int i = 0; i < g_todo_count; i++) {
        todo_t *it = &g_todos[i];
        stats->total++;
        if (strcmp(it->status, "open") == 0) stats->open++;
        else if (strcmp(it->status, "closed") == 0) stats->closed++;
        else if (strcmp(it->status, "archived") == 0) stats->archived++;

        /* 过期：截止日期已过且状态为 open */
        if (it->due_date > 0 && it->due_date < now && strcmp(it->status, "open") == 0) {
            stats->overdue++;
        }
        /* 今日到期 */
        if (it->due_date >= today_start && it->due_date < today_end && strcmp(it->status, "open") == 0) {
            stats->due_today++;
        }
    }

    /* 完成率 = closed / (open + closed)，忽略 archived */
    int active = stats->open + stats->closed;
    stats->completion_rate = active > 0 ? (double)stats->closed / active : 0.0;

    return 0;
}

/* ============================================================
 * Checklist
 * ============================================================ */
int checklist_add(int64_t todo_id, const char *text, checklist_item_t *item) {
    if (find_todo_idx(todo_id) < 0) return -1;

    int max_sort = 0;
    for (int i = 0; i < g_check_count; i++) {
        if (g_checks[i].todo_id == todo_id && g_checks[i].sort_order > max_sort) {
            max_sort = g_checks[i].sort_order;
        }
    }

    ENSURE_CHECK_CAP(g_check_count + 1);
    checklist_item_t *c = &g_checks[g_check_count];
    memset(c, 0, sizeof(*c));
    c->id = g_next_check_id++;
    c->todo_id = todo_id;
    c->done = 0;
    c->sort_order = max_sort + 1;
    strncpy(c->text, text, CHECKLIST_TEXT_MAX - 1);
    g_check_count++;

    if (item) *item = *c;
    todo_db_save();
    return 0;
}

int checklist_toggle(int64_t item_id) {
    int idx = find_check_idx(item_id);
    if (idx < 0) return -1;
    g_checks[idx].done = g_checks[idx].done ? 0 : 1;
    todo_db_save();
    return 0;
}

int checklist_delete(int64_t item_id) {
    int idx = find_check_idx(item_id);
    if (idx < 0) return -1;
    for (int i = idx; i < g_check_count - 1; i++) {
        g_checks[i] = g_checks[i+1];
    }
    g_check_count--;
    todo_db_save();
    return 0;
}

int checklist_list(int64_t todo_id, checklist_item_t **items, int *count) {
    checklist_item_t *list = NULL;
    int cnt = 0, cap = 0;
    for (int i = 0; i < g_check_count; i++) {
        if (g_checks[i].todo_id != todo_id) continue;
        if (cnt >= cap) {
            cap = cap ? cap * 2 : 8;
            list = realloc(list, cap * sizeof(checklist_item_t));
        }
        list[cnt++] = g_checks[i];
    }
    *items = list;
    *count = cnt;
    return 0;
}

void checklist_list_free(checklist_item_t *items, int count) {
    (void)count;
    if (items) free(items);
}

/* ============================================================
 * 分组 CRUD
 * ============================================================ */
int group_create(const group_t *group, int64_t *out_id) {
    ENSURE_GROUP_CAP(g_group_count + 1);
    group_t *g = &g_groups[g_group_count];
    memset(g, 0, sizeof(*g));
    g->id = g_next_group_id++;

    strncpy(g->name, group->name, GROUP_NAME_MAX - 1);
    if (group->color[0])
        strncpy(g->color, group->color, GROUP_COLOR_MAX - 1);
    else
        strcpy(g->color, "#4A90D9");
    g->sort_order = group->sort_order;
    g->created_at = now_ts();

    g_group_count++;
    if (out_id) *out_id = g->id;
    todo_db_save();
    return 0;
}

int group_get(int64_t id, group_t *group) {
    int idx = find_group_idx(id);
    if (idx < 0) return -1;
    if (group) *group = g_groups[idx];
    return 0;
}

int group_update(const group_t *group) {
    int idx = find_group_idx(group->id);
    if (idx < 0) return -1;
    group_t *g = &g_groups[idx];

    strncpy(g->name, group->name, GROUP_NAME_MAX - 1);
    strncpy(g->color, group->color, GROUP_COLOR_MAX - 1);
    g->sort_order = group->sort_order;
    todo_db_save();
    return 0;
}

int group_delete(int64_t id) {
    int idx = find_group_idx(id);
    if (idx < 0) return -1;

    /* 将该分组的待办置为未分组 */
    for (int i = 0; i < g_todo_count; i++) {
        if (g_todos[i].group_id == id) {
            g_todos[i].group_id = 0;
        }
    }

    /* 删除分组 */
    for (int i = idx; i < g_group_count - 1; i++) {
        g_groups[i] = g_groups[i+1];
    }
    g_group_count--;
    todo_db_save();
    return 0;
}

int group_list(group_t **groups, int *count) {
    group_t *list = malloc(g_group_count * sizeof(group_t));
    if (!list) return -1;
    for (int i = 0; i < g_group_count; i++) {
        list[i] = g_groups[i];
    }
    *groups = list;
    *count = g_group_count;
    return 0;
}

void group_list_free(group_t *groups, int count) {
    (void)count;
    if (groups) free(groups);
}

/* ============================================================
 * 评论 CRUD
 * ============================================================ */
int comment_add(int64_t todo_id, const char *text, comment_t *comment) {
    if (find_todo_idx(todo_id) < 0) return -1;

    ENSURE_COMM_CAP(g_comment_count + 1);
    comment_t *c = &g_comments[g_comment_count];
    memset(c, 0, sizeof(*c));
    c->id = g_next_comment_id++;
    c->todo_id = todo_id;
    strncpy(c->text, text, COMMENT_TEXT_MAX - 1);
    c->created_at = now_ts();

    g_comment_count++;
    if (comment) *comment = *c;
    todo_db_save();
    return 0;
}

int comment_list(int64_t todo_id, comment_t **comments, int *count) {
    comment_t *list = NULL;
    int cnt = 0, cap = 0;
    for (int i = 0; i < g_comment_count; i++) {
        if (g_comments[i].todo_id != todo_id) continue;
        if (cnt >= cap) {
            cap = cap ? cap * 2 : 8;
            list = realloc(list, cap * sizeof(comment_t));
        }
        list[cnt++] = g_comments[i];
    }
    *comments = list;
    *count = cnt;
    return 0;
}

void comment_list_free(comment_t *comments, int count) {
    (void)count;
    if (comments) free(comments);
}

int comment_delete(int64_t comment_id) {
    int idx = find_comment_idx(comment_id);
    if (idx < 0) return -1;
    for (int i = idx; i < g_comment_count - 1; i++) {
        g_comments[i] = g_comments[i+1];
    }
    g_comment_count--;
    todo_db_save();
    return 0;
}

/* ============================================================
 * 任务系统 CRUD
 * ============================================================ */
int task_system_create(const task_system_t *ts, int64_t *out_id) {
    ENSURE_TS_CAP(g_ts_count + 1);
    task_system_t *t = &g_task_systems[g_ts_count];
    memset(t, 0, sizeof(*t));
    t->id = g_next_ts_id++;

    strncpy(t->name, ts->name, TASK_SYSTEM_NAME_MAX - 1);
    strncpy(t->description, ts->description, 1023);
    if (ts->color[0])
        strncpy(t->color, ts->color, 15);
    else
        strcpy(t->color, "#4A90D9");
    t->sort_order = ts->sort_order;
    t->created_at = now_ts();

    g_ts_count++;
    if (out_id) *out_id = t->id;
    todo_db_save();
    return 0;
}

int task_system_get(int64_t id, task_system_t *ts) {
    int idx = find_ts_idx(id);
    if (idx < 0) return -1;
    if (ts) *ts = g_task_systems[idx];
    return 0;
}

int task_system_update(const task_system_t *ts) {
    int idx = find_ts_idx(ts->id);
    if (idx < 0) return -1;
    task_system_t *t = &g_task_systems[idx];

    strncpy(t->name, ts->name, TASK_SYSTEM_NAME_MAX - 1);
    strncpy(t->description, ts->description, 1023);
    strncpy(t->color, ts->color, 15);
    t->sort_order = ts->sort_order;
    todo_db_save();
    return 0;
}

int task_system_delete(int64_t id) {
    int idx = find_ts_idx(id);
    if (idx < 0) return -1;

    /* 将该系统的待办置为默认系统 */
    for (int i = 0; i < g_todo_count; i++) {
        if (g_todos[i].task_system_id == id) {
            g_todos[i].task_system_id = 1; /* 默认系统 */
        }
    }

    for (int i = idx; i < g_ts_count - 1; i++) {
        g_task_systems[i] = g_task_systems[i+1];
    }
    g_ts_count--;
    todo_db_save();
    return 0;
}

int task_system_list(task_system_t **systems, int *count) {
    task_system_t *list = malloc(g_ts_count * sizeof(task_system_t));
    if (!list) return -1;
    for (int i = 0; i < g_ts_count; i++) {
        list[i] = g_task_systems[i];
    }
    *systems = list;
    *count = g_ts_count;
    return 0;
}

void task_system_list_free(task_system_t *systems, int count) {
    (void)count;
    if (systems) free(systems);
}

/* ============================================================
 * 学习计划 CRUD
 * ============================================================ */
int plan_create(const plan_t *plan, int64_t *out_id) {
    ENSURE_PLAN_CAP(g_plan_count + 1);
    plan_t *p = &g_plans[g_plan_count];
    memset(p, 0, sizeof(*p));
    p->id = g_next_plan_id++;

    strncpy(p->name, plan->name, PLAN_NAME_MAX - 1);
    strncpy(p->description, plan->description, PLAN_DESC_MAX - 1);
    p->start_date = plan->start_date;
    p->end_date = plan->end_date;
    if (plan->color[0])
        strncpy(p->color, plan->color, 15);
    else
        strcpy(p->color, "#4A90D9");
    p->status = plan->status;
    p->created_at = now_ts();
    p->updated_at = p->created_at;

    g_plan_count++;
    if (out_id) *out_id = p->id;
    todo_db_save();
    return 0;
}

int plan_get(int64_t id, plan_t *plan) {
    int idx = find_plan_idx(id);
    if (idx < 0) return -1;
    if (plan) *plan = g_plans[idx];
    return 0;
}

int plan_update(const plan_t *plan) {
    int idx = find_plan_idx(plan->id);
    if (idx < 0) return -1;
    plan_t *p = &g_plans[idx];

    strncpy(p->name, plan->name, PLAN_NAME_MAX - 1);
    strncpy(p->description, plan->description, PLAN_DESC_MAX - 1);
    p->start_date = plan->start_date;
    p->end_date = plan->end_date;
    strncpy(p->color, plan->color, 15);
    p->status = plan->status;
    p->updated_at = now_ts();
    todo_db_save();
    return 0;
}

int plan_delete(int64_t id) {
    int idx = find_plan_idx(id);
    if (idx < 0) return -1;

    /* 删除关联的 plan_item */
    int j = 0;
    for (int i = 0; i < g_pi_count; i++) {
        if (g_plan_items[i].plan_id != id) {
            g_plan_items[j++] = g_plan_items[i];
        }
    }
    g_pi_count = j;

    /* 删除计划 */
    for (int i = idx; i < g_plan_count - 1; i++) {
        g_plans[i] = g_plans[i+1];
    }
    g_plan_count--;
    todo_db_save();
    return 0;
}

int plan_list(plan_t **plans, int *count) {
    plan_t *list = malloc(g_plan_count * sizeof(plan_t));
    if (!list) return -1;
    for (int i = 0; i < g_plan_count; i++) {
        list[i] = g_plans[i];
    }
    *plans = list;
    *count = g_plan_count;
    return 0;
}

void plan_list_free(plan_t *plans, int count) {
    (void)count;
    if (plans) free(plans);
}

/* ============================================================
 * 计划项 CRUD
 * ============================================================ */
int plan_item_create(const plan_item_t *item, int64_t *out_id) {
    ENSURE_PI_CAP(g_pi_count + 1);
    plan_item_t *pi = &g_plan_items[g_pi_count];
    memset(pi, 0, sizeof(*pi));
    pi->id = g_next_pi_id++;

    pi->plan_id = item->plan_id;
    pi->parent_id = item->parent_id;
    strncpy(pi->title, item->title, ITEM_TITLE_MAX - 1);
    pi->item_type = item->item_type;
    pi->planned_date = item->planned_date;
    pi->estimated_minutes = item->estimated_minutes;
    pi->order_index = item->order_index;
    pi->completion_rule = item->completion_rule;
    pi->todo_id = item->todo_id;
    pi->actual_minutes = item->actual_minutes;

    g_pi_count++;
    if (out_id) *out_id = pi->id;
    todo_db_save();
    return 0;
}

int plan_item_get(int64_t id, plan_item_t *item) {
    int idx = find_pi_idx(id);
    if (idx < 0) return -1;
    if (item) *item = g_plan_items[idx];
    return 0;
}

int plan_item_update(const plan_item_t *item) {
    int idx = find_pi_idx(item->id);
    if (idx < 0) return -1;
    plan_item_t *pi = &g_plan_items[idx];

    pi->plan_id = item->plan_id;
    pi->parent_id = item->parent_id;
    strncpy(pi->title, item->title, ITEM_TITLE_MAX - 1);
    pi->item_type = item->item_type;
    pi->planned_date = item->planned_date;
    pi->estimated_minutes = item->estimated_minutes;
    pi->order_index = item->order_index;
    pi->completion_rule = item->completion_rule;
    pi->todo_id = item->todo_id;
    pi->actual_minutes = item->actual_minutes;
    todo_db_save();
    return 0;
}

int plan_item_delete(int64_t id) {
    int idx = find_pi_idx(id);
    if (idx < 0) return -1;
    for (int i = idx; i < g_pi_count - 1; i++) {
        g_plan_items[i] = g_plan_items[i+1];
    }
    g_pi_count--;
    todo_db_save();
    return 0;
}

int plan_item_list_by_plan(int64_t plan_id, plan_item_t **items, int *count) {
    plan_item_t *list = NULL;
    int cnt = 0, cap = 0;
    for (int i = 0; i < g_pi_count; i++) {
        if (g_plan_items[i].plan_id != plan_id) continue;
        if (cnt >= cap) {
            cap = cap ? cap * 2 : 8;
            list = realloc(list, cap * sizeof(plan_item_t));
        }
        list[cnt++] = g_plan_items[i];
    }
    *items = list;
    *count = cnt;
    return 0;
}

void plan_item_list_free(plan_item_t *items, int count) {
    (void)count;
    if (items) free(items);
}
