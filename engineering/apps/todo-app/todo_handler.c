#include "todo_handler.h"
#include "todo_model.h"
#include "todo_change.h"
#include "cjson/cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

static SOCKET g_listen_socket = INVALID_SOCKET;
static volatile bool g_running = false;

void todo_handler_init(void) {
    /* 数据通过全局 in-memory 表维护 */
}

/* ============================================================
 * JSON 响应工具
 * ============================================================ */
static char *build_json_response(int code, cJSON *data, const char *msg) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "code", code);
    cJSON_AddItemToObject(root, "data", data ? data : cJSON_CreateNull());
    cJSON_AddStringToObject(root, "msg", msg ? msg : "ok");
    char *str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return str;
}

static void send_http_response(SOCKET client, int status_code, const char *status_msg,
                                const char *body) {
    char header[4096];
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, PATCH, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n",
        status_code, status_msg,
        body ? strlen(body) : 0);
    send(client, header, len, 0);
    if (body) send(client, body, (int)strlen(body), 0);
}

static void send_success(SOCKET client, cJSON *data) {
    char *body = build_json_response(0, data, "ok");
    send_http_response(client, 200, "OK", body);
    free(body);
}

static void send_error(SOCKET client, int code, const char *msg) {
    char *body = build_json_response(code, NULL, msg);
    int http_code = (code == 2001) ? 404 : 400;
    const char *http_msg = (code == 2001) ? "Not Found" : "Bad Request";
    send_http_response(client, http_code, http_msg, body);
    free(body);
}

/* ============================================================
 * 序列化函数
 * ============================================================ */
static cJSON *todo_to_json(const todo_t *todo) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "id", todo->id);
    cJSON_AddStringToObject(j, "title", todo->title);
    cJSON_AddStringToObject(j, "description", todo->description);
    cJSON_AddStringToObject(j, "status", todo->status);
    cJSON_AddNumberToObject(j, "priority", todo->priority);
    cJSON_AddNumberToObject(j, "due_date", todo->due_date);
    cJSON_AddNumberToObject(j, "group_id", todo->group_id);
    cJSON_AddNumberToObject(j, "sort_order", todo->sort_order);

    cJSON *labels = cJSON_Parse(todo->labels);
    if (labels && cJSON_IsArray(labels)) {
        cJSON_AddItemToObject(j, "labels", labels);
    } else {
        cJSON_AddItemToObject(j, "labels", cJSON_CreateArray());
        if (labels) cJSON_Delete(labels);
    }

    cJSON_AddNumberToObject(j, "created_at", todo->created_at);
    cJSON_AddNumberToObject(j, "updated_at", todo->updated_at);
    return j;
}

static cJSON *checklist_to_json(const checklist_item_t *item) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "id", item->id);
    cJSON_AddNumberToObject(j, "todo_id", item->todo_id);
    cJSON_AddStringToObject(j, "text", item->text);
    cJSON_AddBoolToObject(j, "done", item->done ? 1 : 0);
    cJSON_AddNumberToObject(j, "sort_order", item->sort_order);
    return j;
}

static cJSON *group_to_json(const group_t *group) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "id", group->id);
    cJSON_AddStringToObject(j, "name", group->name);
    cJSON_AddStringToObject(j, "color", group->color);
    cJSON_AddNumberToObject(j, "sort_order", group->sort_order);
    cJSON_AddNumberToObject(j, "created_at", group->created_at);
    return j;
}

static cJSON *comment_to_json(const comment_t *comment) {
    cJSON *j = cJSON_CreateObject();
    cJSON_AddNumberToObject(j, "id", comment->id);
    cJSON_AddNumberToObject(j, "todo_id", comment->todo_id);
    cJSON_AddStringToObject(j, "text", comment->text);
    cJSON_AddNumberToObject(j, "created_at", comment->created_at);
    return j;
}

/* ============================================================
 * URL 解析
 * ============================================================ */
static void parse_url(const char *url, char *path, size_t path_size,
                      char *query, size_t query_size) {
    const char *q = strchr(url, '?');
    if (q) {
        size_t len = q - url;
        if (len >= path_size) len = path_size - 1;
        memcpy(path, url, len);
        path[len] = '\0';
        strncpy(query, q + 1, query_size - 1);
        query[query_size - 1] = '\0';
    } else {
        strncpy(path, url, path_size - 1);
        path[path_size - 1] = '\0';
        query[0] = '\0';
    }
}

static const char *get_query_param(const char *query, const char *name) {
    if (!query || !query[0]) return NULL;
    static char buf[256];
    const char *p = query;
    size_t nlen = strlen(name);
    while (*p) {
        if (strncmp(p, name, nlen) == 0 && p[nlen] == '=') {
            p += nlen + 1;
            size_t i = 0;
            while (*p && *p != '&' && i < sizeof(buf) - 1) {
                buf[i++] = *p++;
            }
            buf[i] = '\0';
            return buf;
        }
        while (*p && *p != '&') p++;
        if (*p == '&') p++;
    }
    return NULL;
}

/* ============================================================
 * GET /api/todos - 列表
 * ============================================================ */
static void handle_list(SOCKET client, const char *query_str) {
    todo_query_t query;
    memset(&query, 0, sizeof(query));

    query.status = get_query_param(query_str, "status");
    query.labels = get_query_param(query_str, "labels");
    query.search = get_query_param(query_str, "search");

    const char *p_str = get_query_param(query_str, "priority");
    query.priority = p_str ? atoi(p_str) : -1;

    const char *gid_str = get_query_param(query_str, "group_id");
    query.group_id = gid_str ? atoll(gid_str) : -1;

    const char *due_before_str = get_query_param(query_str, "due_before");
    query.due_before = due_before_str ? atoll(due_before_str) : 0;

    const char *due_after_str = get_query_param(query_str, "due_after");
    query.due_after = due_after_str ? atoll(due_after_str) : 0;

    query.sort = get_query_param(query_str, "sort");
    const char *sort_desc_str = get_query_param(query_str, "sort_desc");
    query.sort_desc = sort_desc_str ? atoi(sort_desc_str) : 0;

    const char *page_str = get_query_param(query_str, "page");
    const char *per_page_str = get_query_param(query_str, "per_page");
    query.page = page_str ? atoi(page_str) : 1;
    query.per_page = per_page_str ? atoi(per_page_str) : 20;
    if (query.page < 1) query.page = 1;
    if (query.per_page < 1) query.per_page = 20;
    if (query.per_page > 100) query.per_page = 100;

    todo_list_t result;
    memset(&result, 0, sizeof(result));

    if (todo_list(&query, &result) != 0) {
        send_error(client, 9001, "数据库操作失败");
        return;
    }

    cJSON *jitems = cJSON_CreateArray();
    for (int i = 0; i < result.count; i++) {
        cJSON_AddItemToArray(jitems, todo_to_json(&result.items[i]));
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddItemToObject(data, "items", jitems);
    cJSON_AddNumberToObject(data, "total", result.total);
    cJSON_AddNumberToObject(data, "page", query.page);
    cJSON_AddNumberToObject(data, "per_page", query.per_page);

    todo_list_free(&result);
    send_success(client, data);
}

/* ============================================================
 * GET /api/todos/stats - 统计
 * ============================================================ */
static void handle_stats(SOCKET client) {
    todo_stats_t stats;
    if (todo_get_stats(&stats) != 0) {
        send_error(client, 9001, "统计失败");
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "total", stats.total);
    cJSON_AddNumberToObject(data, "open", stats.open);
    cJSON_AddNumberToObject(data, "closed", stats.closed);
    cJSON_AddNumberToObject(data, "archived", stats.archived);
    cJSON_AddNumberToObject(data, "overdue", stats.overdue);
    cJSON_AddNumberToObject(data, "due_today", stats.due_today);
    cJSON_AddNumberToObject(data, "completion_rate", stats.completion_rate);

    send_success(client, data);
}

/* ============================================================
 * POST /api/todos - 创建
 * ============================================================ */
static void handle_create(SOCKET client, const char *body) {
    if (!body || !body[0]) {
        send_error(client, 1001, "请求体为空");
        return;
    }

    cJSON *jbody = cJSON_Parse(body);
    if (!jbody) {
        send_error(client, 1002, "JSON 解析失败");
        return;
    }

    cJSON *jtitle = cJSON_GetObjectItem(jbody, "title");
    if (!jtitle || !cJSON_IsString(jtitle) || !jtitle->valuestring[0]) {
        cJSON_Delete(jbody);
        send_error(client, 1001, "缺少必填参数: title");
        return;
    }

    todo_t todo;
    memset(&todo, 0, sizeof(todo));
    strncpy(todo.title, jtitle->valuestring, TODO_TITLE_MAX - 1);

    cJSON *jdesc = cJSON_GetObjectItem(jbody, "description");
    if (jdesc && cJSON_IsString(jdesc))
        strncpy(todo.description, jdesc->valuestring, TODO_DESC_MAX - 1);

    cJSON *jlabels = cJSON_GetObjectItem(jbody, "labels");
    if (jlabels && cJSON_IsArray(jlabels)) {
        char *labels_str = cJSON_PrintUnformatted(jlabels);
        if (labels_str) {
            strncpy(todo.labels, labels_str, TODO_LABELS_MAX - 1);
            free(labels_str);
        }
    } else {
        strcpy(todo.labels, "[]");
    }

    cJSON *jpriority = cJSON_GetObjectItem(jbody, "priority");
    todo.priority = jpriority ? (int)jpriority->valuedouble : PRIORITY_NONE;

    cJSON *jdue = cJSON_GetObjectItem(jbody, "due_date");
    todo.due_date = jdue ? (int64_t)jdue->valuedouble : 0;

    cJSON *jgid = cJSON_GetObjectItem(jbody, "group_id");
    todo.group_id = jgid ? (int64_t)jgid->valuedouble : 0;

    cJSON *jso = cJSON_GetObjectItem(jbody, "sort_order");
    todo.sort_order = jso ? (int)jso->valuedouble : 0;

    cJSON *jstatus = cJSON_GetObjectItem(jbody, "status");
    if (jstatus && cJSON_IsString(jstatus))
        strncpy(todo.status, jstatus->valuestring, TODO_STATUS_MAX - 1);
    else
        strcpy(todo.status, "open");

    int64_t new_id = 0;
    if (todo_create(&todo, &new_id) != 0) {
        cJSON_Delete(jbody);
        send_error(client, 9001, "创建失败");
        return;
    }

    todo.id = new_id;
    cJSON *jresult = todo_to_json(&todo);
    cJSON_Delete(jbody);
    send_success(client, jresult);
}

/* ============================================================
 * GET /api/todos/:id - 获取单个
 * ============================================================ */
static void handle_get(SOCKET client, int64_t id) {
    todo_t todo;
    if (todo_get_by_id(id, &todo) != 0) {
        send_error(client, 2001, "todo not found");
        return;
    }

    cJSON *jtodo = todo_to_json(&todo);

    /* 附加 checklist */
    checklist_item_t *items = NULL;
    int count = 0;
    if (checklist_list(id, &items, &count) == 0) {
        cJSON *jchecklist = cJSON_CreateArray();
        for (int i = 0; i < count; i++)
            cJSON_AddItemToArray(jchecklist, checklist_to_json(&items[i]));
        cJSON_AddItemToObject(jtodo, "checklist", jchecklist);
        checklist_list_free(items, count);
    } else {
        cJSON_AddItemToObject(jtodo, "checklist", cJSON_CreateArray());
    }

    /* 附加评论 */
    comment_t *comments = NULL;
    int ccount = 0;
    if (comment_list(id, &comments, &ccount) == 0) {
        cJSON *jcomments = cJSON_CreateArray();
        for (int i = 0; i < ccount; i++)
            cJSON_AddItemToArray(jcomments, comment_to_json(&comments[i]));
        cJSON_AddItemToObject(jtodo, "comments", jcomments);
        comment_list_free(comments, ccount);
    } else {
        cJSON_AddItemToObject(jtodo, "comments", cJSON_CreateArray());
    }

    send_success(client, jtodo);
}

/* ============================================================
 * PATCH /api/todos/:id - 更新
 * ============================================================ */
static void handle_update(SOCKET client, int64_t id, const char *body) {
    todo_t todo;
    if (todo_get_by_id(id, &todo) != 0) {
        send_error(client, 2001, "todo not found");
        return;
    }

    if (body && body[0]) {
        cJSON *jbody = cJSON_Parse(body);
        if (!jbody) {
            send_error(client, 1002, "JSON 解析失败");
            return;
        }

        cJSON *jtitle = cJSON_GetObjectItem(jbody, "title");
        if (jtitle && cJSON_IsString(jtitle))
            strncpy(todo.title, jtitle->valuestring, TODO_TITLE_MAX - 1);

        cJSON *jdesc = cJSON_GetObjectItem(jbody, "description");
        if (jdesc && cJSON_IsString(jdesc))
            strncpy(todo.description, jdesc->valuestring, TODO_DESC_MAX - 1);

        cJSON *jstatus = cJSON_GetObjectItem(jbody, "status");
        if (jstatus && cJSON_IsString(jstatus))
            strncpy(todo.status, jstatus->valuestring, TODO_STATUS_MAX - 1);

        cJSON *jlabels = cJSON_GetObjectItem(jbody, "labels");
        if (jlabels && cJSON_IsArray(jlabels)) {
            char *labels_str = cJSON_PrintUnformatted(jlabels);
            if (labels_str) {
                strncpy(todo.labels, labels_str, TODO_LABELS_MAX - 1);
                free(labels_str);
            }
        }

        cJSON *jpriority = cJSON_GetObjectItem(jbody, "priority");
        if (jpriority) todo.priority = (int)jpriority->valuedouble;

        cJSON *jdue = cJSON_GetObjectItem(jbody, "due_date");
        if (jdue) todo.due_date = (int64_t)jdue->valuedouble;

        cJSON *jgid = cJSON_GetObjectItem(jbody, "group_id");
        if (jgid) todo.group_id = (int64_t)jgid->valuedouble;

        cJSON *jso = cJSON_GetObjectItem(jbody, "sort_order");
        if (jso) todo.sort_order = (int)jso->valuedouble;

        cJSON_Delete(jbody);
    }

    if (todo_update(&todo) != 0) {
        send_error(client, 9001, "更新失败");
        return;
    }

    cJSON *jresult = todo_to_json(&todo);
    send_success(client, jresult);
}

/* ============================================================
 * DELETE /api/todos/:id - 删除
 * ============================================================ */
static void handle_delete(SOCKET client, int64_t id) {
    if (todo_delete(id) != 0) {
        send_error(client, 2001, "todo not found");
        return;
    }
    send_success(client, cJSON_CreateObject());
}

/* ============================================================
 * PATCH /api/todos/:id/sort - 更新排序
 * ============================================================ */
static void handle_sort(SOCKET client, int64_t id, const char *body) {
    int sort_order = 0;
    if (body && body[0]) {
        cJSON *jbody = cJSON_Parse(body);
        if (jbody) {
            cJSON *jso = cJSON_GetObjectItem(jbody, "sort_order");
            if (jso) sort_order = (int)jso->valuedouble;
            cJSON_Delete(jbody);
        }
    }

    if (todo_update_sort(id, sort_order) != 0) {
        send_error(client, 2001, "todo not found");
        return;
    }
    send_success(client, cJSON_CreateObject());
}

/* ============================================================
 * POST /api/todos/:id/create-change - OPSX 变更
 * ============================================================ */
static void handle_create_change(SOCKET client, int64_t id) {
    todo_t todo;
    if (todo_get_by_id(id, &todo) != 0) {
        send_error(client, 2001, "todo not found");
        return;
    }

    char change_id[128] = {0};
    char change_url[512] = {0};
    int ret = todo_create_change(id, change_id, sizeof(change_id), change_url, sizeof(change_url));

    if (ret != 0) {
        send_error(client, 9002, "创建变更失败");
        return;
    }

    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "change_id", change_id);
    cJSON_AddStringToObject(data, "url", change_url);
    send_success(client, data);
}

/* ============================================================
 * Checklist
 * ============================================================ */
static void handle_checklist_add(SOCKET client, int64_t todo_id, const char *body) {
    if (!body || !body[0]) {
        send_error(client, 1001, "请求体为空");
        return;
    }

    cJSON *jbody = cJSON_Parse(body);
    if (!jbody) {
        send_error(client, 1002, "JSON 解析失败");
        return;
    }

    cJSON *jtext = cJSON_GetObjectItem(jbody, "text");
    if (!jtext || !cJSON_IsString(jtext) || !jtext->valuestring[0]) {
        cJSON_Delete(jbody);
        send_error(client, 1001, "缺少必填参数: text");
        return;
    }

    checklist_item_t item;
    memset(&item, 0, sizeof(item));
    if (checklist_add(todo_id, jtext->valuestring, &item) != 0) {
        cJSON_Delete(jbody);
        send_error(client, 2001, "todo not found");
        return;
    }

    cJSON_Delete(jbody);
    send_success(client, checklist_to_json(&item));
}

static void handle_checklist_toggle(SOCKET client, int64_t todo_id, int64_t item_id) {
    (void)todo_id;
    if (checklist_toggle(item_id) != 0) {
        send_error(client, 2001, "checklist item not found");
        return;
    }
    send_success(client, cJSON_CreateObject());
}

static void handle_checklist_delete(SOCKET client, int64_t todo_id, int64_t item_id) {
    (void)todo_id;
    if (checklist_delete(item_id) != 0) {
        send_error(client, 2001, "checklist item not found");
        return;
    }
    send_success(client, cJSON_CreateObject());
}

/* ============================================================
 * 评论
 * ============================================================ */
static void handle_comment_add(SOCKET client, int64_t todo_id, const char *body) {
    if (!body || !body[0]) {
        send_error(client, 1001, "请求体为空");
        return;
    }

    cJSON *jbody = cJSON_Parse(body);
    if (!jbody) {
        send_error(client, 1002, "JSON 解析失败");
        return;
    }

    cJSON *jtext = cJSON_GetObjectItem(jbody, "text");
    if (!jtext || !cJSON_IsString(jtext) || !jtext->valuestring[0]) {
        cJSON_Delete(jbody);
        send_error(client, 1001, "缺少必填参数: text");
        return;
    }

    comment_t comment;
    memset(&comment, 0, sizeof(comment));
    if (comment_add(todo_id, jtext->valuestring, &comment) != 0) {
        cJSON_Delete(jbody);
        send_error(client, 2001, "todo not found");
        return;
    }

    cJSON_Delete(jbody);
    send_success(client, comment_to_json(&comment));
}

static void handle_comment_list(SOCKET client, int64_t todo_id) {
    comment_t *comments = NULL;
    int count = 0;
    if (comment_list(todo_id, &comments, &count) != 0) {
        send_error(client, 9001, "查询失败");
        return;
    }

    cJSON *jcomments = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(jcomments, comment_to_json(&comments[i]));
    }
    comment_list_free(comments, count);
    send_success(client, jcomments);
}

static void handle_comment_delete(SOCKET client, int64_t todo_id, int64_t comment_id) {
    (void)todo_id;
    if (comment_delete(comment_id) != 0) {
        send_error(client, 2001, "comment not found");
        return;
    }
    send_success(client, cJSON_CreateObject());
}

/* ============================================================
 * 分组 CRUD
 * ============================================================ */
static void handle_group_list(SOCKET client) {
    group_t *groups = NULL;
    int count = 0;
    if (group_list(&groups, &count) != 0) {
        send_error(client, 9001, "查询失败");
        return;
    }

    cJSON *jgroups = cJSON_CreateArray();
    for (int i = 0; i < count; i++) {
        cJSON_AddItemToArray(jgroups, group_to_json(&groups[i]));
    }
    group_list_free(groups, count);
    send_success(client, jgroups);
}

static void handle_group_create(SOCKET client, const char *body) {
    if (!body || !body[0]) {
        send_error(client, 1001, "请求体为空");
        return;
    }

    cJSON *jbody = cJSON_Parse(body);
    if (!jbody) {
        send_error(client, 1002, "JSON 解析失败");
        return;
    }

    cJSON *jname = cJSON_GetObjectItem(jbody, "name");
    if (!jname || !cJSON_IsString(jname) || !jname->valuestring[0]) {
        cJSON_Delete(jbody);
        send_error(client, 1001, "缺少必填参数: name");
        return;
    }

    group_t group;
    memset(&group, 0, sizeof(group));
    strncpy(group.name, jname->valuestring, GROUP_NAME_MAX - 1);

    cJSON *jcolor = cJSON_GetObjectItem(jbody, "color");
    if (jcolor && cJSON_IsString(jcolor))
        strncpy(group.color, jcolor->valuestring, GROUP_COLOR_MAX - 1);
    else
        strcpy(group.color, "#4A90D9");

    cJSON *jso = cJSON_GetObjectItem(jbody, "sort_order");
    group.sort_order = jso ? (int)jso->valuedouble : 0;

    int64_t new_id = 0;
    if (group_create(&group, &new_id) != 0) {
        cJSON_Delete(jbody);
        send_error(client, 9001, "创建失败");
        return;
    }

    group.id = new_id;
    cJSON *jresult = group_to_json(&group);
    cJSON_Delete(jbody);
    send_success(client, jresult);
}

static void handle_group_update(SOCKET client, int64_t id, const char *body) {
    group_t group;
    if (group_get(id, &group) != 0) {
        send_error(client, 2001, "group not found");
        return;
    }

    if (body && body[0]) {
        cJSON *jbody = cJSON_Parse(body);
        if (jbody) {
            cJSON *jname = cJSON_GetObjectItem(jbody, "name");
            if (jname && cJSON_IsString(jname))
                strncpy(group.name, jname->valuestring, GROUP_NAME_MAX - 1);

            cJSON *jcolor = cJSON_GetObjectItem(jbody, "color");
            if (jcolor && cJSON_IsString(jcolor))
                strncpy(group.color, jcolor->valuestring, GROUP_COLOR_MAX - 1);

            cJSON *jso = cJSON_GetObjectItem(jbody, "sort_order");
            if (jso) group.sort_order = (int)jso->valuedouble;

            cJSON_Delete(jbody);
        }
    }

    if (group_update(&group) != 0) {
        send_error(client, 9001, "更新失败");
        return;
    }

    cJSON *jresult = group_to_json(&group);
    send_success(client, jresult);
}

static void handle_group_delete(SOCKET client, int64_t id) {
    if (group_delete(id) != 0) {
        send_error(client, 2001, "group not found");
        return;
    }
    send_success(client, cJSON_CreateObject());
}

/* ============================================================
 * 静态文件服务
 * ============================================================ */
static int serve_static_file(SOCKET client, const char *url) {
    char filepath[1024];

    /* 路径遍历防御 */
    if (strstr(url, "..") != NULL) return -1;

    const char *candidates[] = {
        "web%s",
        "engineering/apps/web/todo-app/dist%s",
        "../engineering/apps/web/todo-app/dist%s",
        "../../engineering/apps/web/todo-app/dist%s",
        "apps/web/todo-app/dist%s",
        NULL
    };

    char urlpath[512];
    if (strcmp(url, "/") == 0) {
        snprintf(urlpath, sizeof(urlpath), "/index.html");
    } else {
        snprintf(urlpath, sizeof(urlpath), "%s", url);
    }

    FILE *fp = NULL;
    for (int i = 0; candidates[i]; i++) {
        snprintf(filepath, sizeof(filepath), candidates[i], urlpath);
        fp = fopen(filepath, "rb");
        if (fp) break;
    }
    if (!fp) return -1;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize < 0) { fclose(fp); return -1; }

    char *data = malloc(fsize + 1);
    if (!data) { fclose(fp); return -1; }
    size_t rd = fread(data, 1, fsize, fp);
    fclose(fp);

    const char *mime = "text/plain; charset=utf-8";
    if (strstr(urlpath, ".html")) mime = "text/html; charset=utf-8";
    else if (strstr(urlpath, ".js"))  mime = "application/javascript";
    else if (strstr(urlpath, ".css")) mime = "text/css";
    else if (strstr(urlpath, ".json")) mime = "application/json";
    else if (strstr(urlpath, ".png"))  mime = "image/png";
    else if (strstr(urlpath, ".svg"))  mime = "image/svg+xml";
    else if (strstr(urlpath, ".ico"))  mime = "image/x-icon";

    char header[4096];
    int len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %ld\r\n"
        "Connection: close\r\n"
        "\r\n", mime, (long)rd);
    send(client, header, len, 0);
    send(client, data, (int)rd, 0);
    free(data);
    return 0;
}

/* ============================================================
 * HTTP 请求解析与路由
 * ============================================================ */
static int parse_content_length(const char *request) {
    const char *cl = strstr(request, "Content-Length:");
    if (!cl) cl = strstr(request, "content-length:");
    if (!cl) return 0;
    cl += 15;
    while (*cl == ' ' || *cl == '\t') cl++;
    return atoi(cl);
}

static void handle_request(SOCKET client, const char *request) {
    char method[32], url[1024];
    char path[1024], query[1024];

    /* OPTIONS 预检 */
    if (strncmp(request, "OPTIONS ", 8) == 0) {
        send_http_response(client, 204, "No Content", NULL);
        return;
    }

    /* 解析请求行 */
    const char *space1 = strchr(request, ' ');
    if (!space1) return;
    size_t method_len = space1 - request;
    if (method_len >= sizeof(method)) return;
    memcpy(method, request, method_len);
    method[method_len] = '\0';

    const char *url_start = space1 + 1;
    const char *space2 = strchr(url_start, ' ');
    if (!space2) return;
    size_t url_len = space2 - url_start;
    if (url_len >= sizeof(url)) return;
    memcpy(url, url_start, url_len);
    url[url_len] = '\0';

    /* 解析 body */
    int content_length = parse_content_length(request);
    char *body = NULL;
    char body_small[4096];

    const char *body_start = strstr(request, "\r\n\r\n");
    if (body_start && content_length > 0) {
        body_start += 4;
        size_t available = strlen(body_start);
        size_t to_copy = (size_t)content_length < available ? (size_t)content_length : available;
        if (to_copy >= sizeof(body_small)) {
            body = malloc(to_copy + 1);
            if (body) {
                memcpy(body, body_start, to_copy);
                body[to_copy] = '\0';
            }
        } else {
            memcpy(body_small, body_start, to_copy);
            body_small[to_copy] = '\0';
            body = body_small;
        }
    } else {
        body_small[0] = '\0';
        body = body_small;
    }

    /* 解析 URL */
    parse_url(url, path, sizeof(path), query, sizeof(query));

    int64_t id_a = 0, id_b = 0;
    int handled = 0;

    /* ============================================================
     * 路由表
     * ============================================================ */

    /* GET /api/todos */
    if (strcmp(path, "/api/todos") == 0 && strcmp(method, "GET") == 0) {
        handle_list(client, query);
        handled = 1;
    }
    /* POST /api/todos */
    else if (strcmp(path, "/api/todos") == 0 && strcmp(method, "POST") == 0) {
        handle_create(client, body);
        handled = 1;
    }
    /* GET /api/todos/stats */
    else if (strcmp(path, "/api/todos/stats") == 0 && strcmp(method, "GET") == 0) {
        handle_stats(client);
        handled = 1;
    }
    /* GET /api/todos/:id */
    else if (sscanf(path, "/api/todos/%lld", (long long *)&id_a) == 1) {
        char expect[128];
        snprintf(expect, sizeof(expect), "/api/todos/%lld", (long long)id_a);
        if (strcmp(path, expect) == 0) {
            if (strcmp(method, "GET") == 0) {
                handle_get(client, id_a);
                handled = 1;
            } else if (strcmp(method, "PATCH") == 0) {
                handle_update(client, id_a, body);
                handled = 1;
            } else if (strcmp(method, "DELETE") == 0) {
                handle_delete(client, id_a);
                handled = 1;
            }
        }
        /* PATCH /api/todos/:id/sort */
        else if (sscanf(path, "/api/todos/%lld/sort", (long long *)&id_a) == 1) {
            snprintf(expect, sizeof(expect), "/api/todos/%lld/sort", (long long)id_a);
            if (strcmp(path, expect) == 0 && strcmp(method, "PATCH") == 0) {
                handle_sort(client, id_a, body);
                handled = 1;
            }
        }
        /* POST /api/todos/:id/create-change */
        else if (sscanf(path, "/api/todos/%lld/create-change", (long long *)&id_a) == 1) {
            snprintf(expect, sizeof(expect), "/api/todos/%lld/create-change", (long long)id_a);
            if (strcmp(path, expect) == 0 && strcmp(method, "POST") == 0) {
                handle_create_change(client, id_a);
                handled = 1;
            }
        }
        /* POST /api/todos/:id/checklist */
        else if (sscanf(path, "/api/todos/%lld/checklist", (long long *)&id_a) == 1) {
            snprintf(expect, sizeof(expect), "/api/todos/%lld/checklist", (long long)id_a);
            if (strcmp(path, expect) == 0 && strcmp(method, "POST") == 0) {
                handle_checklist_add(client, id_a, body);
                handled = 1;
            }
        }
        /* DELETE /api/todos/:id/checklist/:item_id */
        else if (sscanf(path, "/api/todos/%lld/checklist/%lld", (long long *)&id_a, (long long *)&id_b) == 2) {
            snprintf(expect, sizeof(expect), "/api/todos/%lld/checklist/%lld", (long long)id_a, (long long)id_b);
            if (strcmp(path, expect) == 0 && strcmp(method, "DELETE") == 0) {
                handle_checklist_delete(client, id_a, id_b);
                handled = 1;
            }
        }
        /* PATCH /api/todos/:id/checklist/:item_id */
        else if (sscanf(path, "/api/todos/%lld/checklist/%lld", (long long *)&id_a, (long long *)&id_b) == 2) {
            snprintf(expect, sizeof(expect), "/api/todos/%lld/checklist/%lld", (long long)id_a, (long long)id_b);
            if (strcmp(path, expect) == 0 && strcmp(method, "PATCH") == 0) {
                handle_checklist_toggle(client, id_a, id_b);
                handled = 1;
            }
        }
        /* 评论路由 */
        else if (sscanf(path, "/api/todos/%lld/comments", (long long *)&id_a) == 1) {
            snprintf(expect, sizeof(expect), "/api/todos/%lld/comments", (long long)id_a);
            if (strcmp(path, expect) == 0) {
                if (strcmp(method, "GET") == 0) {
                    handle_comment_list(client, id_a);
                    handled = 1;
                } else if (strcmp(method, "POST") == 0) {
                    handle_comment_add(client, id_a, body);
                    handled = 1;
                }
            }
            /* DELETE /api/todos/:id/comments/:comment_id */
            else if (sscanf(path, "/api/todos/%lld/comments/%lld", (long long *)&id_a, (long long *)&id_b) == 2) {
                snprintf(expect, sizeof(expect), "/api/todos/%lld/comments/%lld", (long long)id_a, (long long)id_b);
                if (strcmp(path, expect) == 0 && strcmp(method, "DELETE") == 0) {
                    handle_comment_delete(client, id_a, id_b);
                    handled = 1;
                }
            }
        }
    }

    /* 分组路由 */
    /* GET /api/groups */
    else if (strcmp(path, "/api/groups") == 0) {
        if (strcmp(method, "GET") == 0) {
            handle_group_list(client);
            handled = 1;
        } else if (strcmp(method, "POST") == 0) {
            handle_group_create(client, body);
            handled = 1;
        }
    }
    /* PATCH/DELETE /api/groups/:id */
    else if (sscanf(path, "/api/groups/%lld", (long long *)&id_a) == 1) {
        char expect[128];
        snprintf(expect, sizeof(expect), "/api/groups/%lld", (long long)id_a);
        if (strcmp(path, expect) == 0) {
            if (strcmp(method, "GET") == 0) {
                group_t g;
                if (group_get(id_a, &g) == 0) {
                    send_success(client, group_to_json(&g));
                } else {
                    send_error(client, 2001, "group not found");
                }
                handled = 1;
            } else if (strcmp(method, "PATCH") == 0) {
                handle_group_update(client, id_a, body);
                handled = 1;
            } else if (strcmp(method, "DELETE") == 0) {
                handle_group_delete(client, id_a);
                handled = 1;
            }
        }
    }

    /* 静态文件或 404 */
    if (!handled) {
        if (serve_static_file(client, path) != 0) {
            send_error(client, 404, "not found");
        }
    }

    if (body && body != body_small) free(body);
}

/* ============================================================
 * HTTP 服务器
 * ============================================================ */
int http_server_start(int port) {
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        fprintf(stderr, "WSAStartup 失败\n");
        return -1;
    }

    g_listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_socket == INVALID_SOCKET) {
        fprintf(stderr, "创建 socket 失败\n");
        WSACleanup();
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(g_listen_socket, (struct sockaddr *)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "绑定端口 %d 失败\n", port);
        closesocket(g_listen_socket);
        WSACleanup();
        return -1;
    }

    if (listen(g_listen_socket, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "监听失败\n");
        closesocket(g_listen_socket);
        WSACleanup();
        return -1;
    }

    g_running = true;
    printf("HTTP 服务器已启动，监听端口: %d\n", port);
    fflush(stdout);

    while (g_running) {
        struct sockaddr_in client_addr;
        int addr_len = sizeof(client_addr);
        SOCKET client = accept(g_listen_socket, (struct sockaddr *)&client_addr, &addr_len);

        if (client == INVALID_SOCKET) {
            if (g_running) Sleep(10);
            continue;
        }

        char request[131072];
        int bytes = recv(client, request, sizeof(request) - 1, 0);
        if (bytes > 0) {
            request[bytes] = '\0';
            handle_request(client, request);
        }
        closesocket(client);
    }

    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
    }
    WSACleanup();
    return 0;
}

void http_server_stop(void) {
    g_running = false;
    if (g_listen_socket != INVALID_SOCKET) {
        closesocket(g_listen_socket);
        g_listen_socket = INVALID_SOCKET;
    }
}
