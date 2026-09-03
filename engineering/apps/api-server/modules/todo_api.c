#include "todo_api.h"
#include "../../common/http_server.h"
#include "../../common/db_pool.h"
#include "../config.h"
#include "todo_model.h"
#include "todo_db.h"
#include "todo_plan.h"
#include "todo_stats.h"
#include "todo_calendar.h"
#include "todo_field.h"
#include "todo_view.h"
#include "todo_change.h"
#include "todo_migration.h"
#include "cjson/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

static int ensure_todo_db(void) {
    if (!todo_db_handle()) {
        if (todo_db_open(g_config.db_path) != 0) {
            fprintf(stderr, "[todo_api] failed to open db: %s\n", g_config.db_path);
            return -1;
        }
        todo_migrate_from_legacy();
    }
    return 0;
}

static cJSON *todo_to_json(const todo_t *t) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "id", (double)t->id);
    cJSON_AddStringToObject(j, "title", t->title);
    cJSON_AddStringToObject(j, "description", t->description);
    cJSON_AddStringToObject(j, "status", t->status);
    cJSON_AddStringToObject(j, "labels", t->labels);
    cJSON_AddNumberToObject(j, "priority", t->priority);
    cJSON_AddNumberToObject(j, "due_date", (double)t->due_date);
    cJSON_AddNumberToObject(j, "group_id", (double)t->group_id);
    cJSON_AddNumberToObject(j, "sort_order", t->sort_order);
    cJSON_AddNumberToObject(j, "created_at", (double)t->created_at);
    cJSON_AddNumberToObject(j, "updated_at", (double)t->updated_at);
    cJSON_AddNumberToObject(j, "todo_type", t->todo_type);
    cJSON_AddNumberToObject(j, "carryover_count", t->carryover_count);
    cJSON_AddNumberToObject(j, "completed_at", (double)t->completed_at);
    return j;
}

static int json_to_todo(cJSON *j, todo_t *t) {
    memset(t, 0, sizeof(*t));
    cJSON *id = cJSON_GetObjectItem(j, "id");
    if (id && cJSON_IsNumber(id)) t->id = (int64_t)cJSON_GetNumberValue(id);
    cJSON *title = cJSON_GetObjectItem(j, "title");
    if (title && cJSON_IsString(title)) strncpy(t->title, cJSON_GetStringValue(title), TODO_TITLE_MAX - 1);
    cJSON *desc = cJSON_GetObjectItem(j, "description");
    if (desc && cJSON_IsString(desc)) strncpy(t->description, cJSON_GetStringValue(desc), TODO_DESC_MAX - 1);
    cJSON *status = cJSON_GetObjectItem(j, "status");
    if (status && cJSON_IsString(status)) strncpy(t->status, cJSON_GetStringValue(status), TODO_STATUS_MAX - 1);
    cJSON *labels = cJSON_GetObjectItem(j, "labels");
    if (labels && cJSON_IsString(labels)) strncpy(t->labels, cJSON_GetStringValue(labels), TODO_LABELS_MAX - 1);
    cJSON *pri = cJSON_GetObjectItem(j, "priority");
    if (pri && cJSON_IsNumber(pri)) t->priority = (int)cJSON_GetNumberValue(pri);
    cJSON *due = cJSON_GetObjectItem(j, "due_date");
    if (due && cJSON_IsNumber(due)) t->due_date = (int64_t)cJSON_GetNumberValue(due);
    cJSON *gid = cJSON_GetObjectItem(j, "group_id");
    if (gid && cJSON_IsNumber(gid)) t->group_id = (int64_t)cJSON_GetNumberValue(gid);
    return 0;
}

static void handle_todo_list(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    if (ensure_todo_db() != 0) { http_respond_error(resp, 500, "db not initialized"); return; }
    todo_query_t query;
    memset(&query, 0, sizeof(query));
    query.status = NULL;
    query.page = 0;
    query.per_page = 0;
    todo_list_t result;
    if (todo_list(&query, &result) != 0) {
        http_respond_error(resp, 500, "query failed");
        return;
    }
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < result.count; i++) {
        cJSON_AddItemToArray(arr, todo_to_json(&result.items[i]));
    }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "todos", arr);
    cJSON_AddNumberToObject(root, "total", result.total);
    char *json = cJSON_PrintUnformatted(root);
    http_respond_json(resp, 200, json);
    cJSON_Delete(root);
    todo_list_free(&result);
    free(json);
}

static void handle_todo_get(const HttpRequest *req, HttpResponse *resp) {
    if (ensure_todo_db() != 0) { http_respond_error(resp, 500, "db not initialized"); return; }
    if (req->param_count < 1) { http_respond_error(resp, 400, "missing id"); return; }
    int64_t id = (int64_t)atoll(req->params[0]);
    todo_t todo;
    if (todo_get_by_id(id, &todo) != 0) { http_respond_error(resp, 404, "not found"); return; }
    cJSON *j = todo_to_json(&todo);
    char *json = cJSON_PrintUnformatted(j);
    http_respond_json(resp, 200, json);
    cJSON_Delete(j);
    free(json);
}

static void handle_todo_create(const HttpRequest *req, HttpResponse *resp) {
    if (ensure_todo_db() != 0) { http_respond_error(resp, 500, "db not initialized"); return; }
    if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
    cJSON *j = cJSON_Parse(req->body);
    if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
    todo_t todo;
    json_to_todo(j, &todo);
    cJSON_Delete(j);
    int64_t out_id;
    if (todo_create(&todo, &out_id) != 0) { http_respond_error(resp, 500, "create failed"); return; }
    char json_buf[128];
    snprintf(json_buf, sizeof(json_buf), "{\"id\":%" PRId64 "}", out_id);
    http_respond_json(resp, 201, json_buf);
}

static void handle_todo_update(const HttpRequest *req, HttpResponse *resp) {
    if (ensure_todo_db() != 0) { http_respond_error(resp, 500, "db not initialized"); return; }
    if (req->param_count < 1) { http_respond_error(resp, 400, "missing id"); return; }
    if (!req->body || !req->body_len) { http_respond_error(resp, 400, "empty body"); return; }
    int64_t id = (int64_t)atoll(req->params[0]);
    cJSON *j = cJSON_Parse(req->body);
    if (!j) { http_respond_error(resp, 400, "invalid json"); return; }
    todo_t todo;
    if (todo_get_by_id(id, &todo) != 0) { cJSON_Delete(j); http_respond_error(resp, 404, "not found"); return; }
    cJSON *title = cJSON_GetObjectItem(j, "title");
    if (title && cJSON_IsString(title)) strncpy(todo.title, cJSON_GetStringValue(title), TODO_TITLE_MAX - 1);
    cJSON *desc = cJSON_GetObjectItem(j, "description");
    if (desc && cJSON_IsString(desc)) strncpy(todo.description, cJSON_GetStringValue(desc), TODO_DESC_MAX - 1);
    cJSON *status = cJSON_GetObjectItem(j, "status");
    if (status && cJSON_IsString(status)) strncpy(todo.status, cJSON_GetStringValue(status), TODO_STATUS_MAX - 1);
    cJSON *pri = cJSON_GetObjectItem(j, "priority");
    if (pri && cJSON_IsNumber(pri)) todo.priority = (int)cJSON_GetNumberValue(pri);
    cJSON *due = cJSON_GetObjectItem(j, "due_date");
    if (due && cJSON_IsNumber(due)) todo.due_date = (int64_t)cJSON_GetNumberValue(due);
    cJSON *gid = cJSON_GetObjectItem(j, "group_id");
    if (gid && cJSON_IsNumber(gid)) todo.group_id = (int64_t)cJSON_GetNumberValue(gid);
    cJSON_Delete(j);
    if (todo_update(&todo) != 0) { http_respond_error(resp, 500, "update failed"); return; }
    http_respond_json(resp, 200, "{\"ok\":true}");
}

static void handle_todo_delete(const HttpRequest *req, HttpResponse *resp) {
    if (ensure_todo_db() != 0) { http_respond_error(resp, 500, "db not initialized"); return; }
    if (req->param_count < 1) { http_respond_error(resp, 400, "missing id"); return; }
    int64_t id = (int64_t)atoll(req->params[0]);
    if (todo_delete(id) != 0) { http_respond_error(resp, 404, "not found"); return; }
    http_respond_json(resp, 200, "{\"ok\":true}");
}

static void handle_group_list(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    if (ensure_todo_db() != 0) { http_respond_error(resp, 500, "db not initialized"); return; }
    group_t *groups = NULL;
    int count = 0;
    if (group_list(&groups, &count) != 0) { http_respond_error(resp, 500, "query failed"); return; }
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON *g = cJSON_CreateObject();
        cJSON_AddNumberToObject(g, "id", (double)groups[i].id);
        cJSON_AddStringToObject(g, "name", groups[i].name);
        cJSON_AddStringToObject(g, "color", groups[i].color);
        cJSON_AddNumberToObject(g, "sort_order", groups[i].sort_order);
        cJSON_AddItemToArray(arr, g);
    }
    group_list_free(groups, count);
    char *json = cJSON_PrintUnformatted(arr);
    http_respond_json(resp, 200, json);
    cJSON_Delete(arr);
    free(json);
}

static void handle_todo_stats(const HttpRequest *req, HttpResponse *resp) {
    (void)req;
    if (ensure_todo_db() != 0) { http_respond_error(resp, 500, "db not initialized"); return; }
    todo_stats_t stats;
    if (todo_get_stats(&stats) != 0) { http_respond_error(resp, 500, "stats failed"); return; }
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "total", stats.total);
    cJSON_AddNumberToObject(j, "open", stats.open);
    cJSON_AddNumberToObject(j, "closed", stats.closed);
    cJSON_AddNumberToObject(j, "archived", stats.archived);
    cJSON_AddNumberToObject(j, "overdue", stats.overdue);
    cJSON_AddNumberToObject(j, "due_today", stats.due_today);
    cJSON_AddNumberToObject(j, "completion_rate", stats.completion_rate);
    char *json = cJSON_PrintUnformatted(j);
    http_respond_json(resp, 200, json);
    cJSON_Delete(j);
    free(json);
}

void register_todo_routes(Router *r) {
    /* 静态路径必须在 :param 路径之前注册 */
    router_add(r, 'G', "/api/todos", handle_todo_list);
    router_add(r, 'G', "/api/todos/stats", handle_todo_stats);
    router_add(r, 'G', "/api/todos/:id", handle_todo_get);
    router_add(r, 'P', "/api/todos", handle_todo_create);
    router_add(r, 'U', "/api/todos/:id", handle_todo_update);
    router_add(r, 'D', "/api/todos/:id", handle_todo_delete);
    router_add(r, 'G', "/api/groups", handle_group_list);
}
